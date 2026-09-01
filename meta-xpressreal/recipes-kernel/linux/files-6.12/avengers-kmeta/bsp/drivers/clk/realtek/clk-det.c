// SPDX-License-Identifier: GPL-2.0-only

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <soc/realtek/rtk_clk_det.h>

struct clk_det_desc {
	u32 ctrl_rstn_bit;
	u32 ctrl_cnten_bit;
	u32 stat_ofs;
	u32 stat_done_bit;
	u32 cnt_ofs;
	u32 cnt_mask;
	u32 cnt_shift;
	const char *const *output_sel;
	int output_sel_num;
	u32 output_sel_offset;
	u32 output_sel_mask;

	u32 no_polling_done : 1;
	u32 separated_cnt_reg : 1;
};

struct clk_det {
	struct device *dev;
	void __iomem *base;
	struct clk_hw hw;
	int ofs;
	struct clk *ref;

	const struct clk_det_desc *desc;
	struct regmap *os_regmap;
};

static const struct clk_det_desc legacy_descs[] = {
	[CLK_DET_TYPE_GENERIC] = {
		.ctrl_rstn_bit = 0,
		.ctrl_cnten_bit = 1,
		.stat_ofs = 0x0,
		.stat_done_bit = 30,
		.cnt_mask = GENMASK(29, 13),
		.cnt_shift = 13,
		.no_polling_done = 0,
	},
	[CLK_DET_TYPE_SC_WRAP] = {
		.ctrl_rstn_bit = 17,
		.ctrl_cnten_bit = 16,
		.stat_ofs = 0x8,
		.stat_done_bit = 0,
		.cnt_mask = GENMASK(17, 1),
		.cnt_shift = 1,
	},
	[CLK_DET_TYPE_HDMI_TOP] = {
		.ctrl_rstn_bit = 0,
		.ctrl_cnten_bit = 1,
		.stat_ofs = 0x0,
		.cnt_mask = GENMASK(29, 13),
		.cnt_shift = 13,
		.no_polling_done = 1,
	}
};

static DEFINE_MUTEX(clk_det_lock);

static const char * const rtd1625_sc_wrap_sels[] = {
	"pll_scpu", "clk_scpu", "clk_dsu",
};

static const struct clk_det_desc rtd1625_sc_wrap_desc = {
	.ctrl_rstn_bit = 17,
	.ctrl_cnten_bit = 16,
	.stat_ofs = 0x8,
	.stat_done_bit = 0,
	.cnt_mask = GENMASK(17, 1),
	.cnt_shift = 1,
	.output_sel = rtd1625_sc_wrap_sels,
	.output_sel_num = ARRAY_SIZE(rtd1625_sc_wrap_sels),
	.output_sel_offset = 0x44c,
	.output_sel_mask = 0x70,
};

static const char * const rtd1635_sc_wrap_sels[] = {
	"pll_scpu", "clk_scpu", NULL, NULL,
	"clk_pllscpu_h0_dsu", "clk_dsu",
	"clk_pllscpu_ph0", "clk_pllscpu_ph0_dsu",
	"pll_big_scpu", "clk_big_scpu", NULL, NULL,
	"clk_pllscpu_big_h0_dsu", "clk_dsu/2",
	"clk_pllscpu_big_ph0", "clk_pllscpu_big_ph0_dsu",
};

static const struct clk_det_desc rtd1635_sc_wrap_desc = {
	.ctrl_rstn_bit = 17,
	.ctrl_cnten_bit = 16,
	.stat_ofs = 0x34,
	.stat_done_bit = 0,
	.separated_cnt_reg = 1,
	.cnt_ofs = 0x8,
	.cnt_mask = GENMASK(18, 0),
	.cnt_shift = 0,
	.output_sel = rtd1635_sc_wrap_sels,
	.output_sel_num = ARRAY_SIZE(rtd1635_sc_wrap_sels),
	.output_sel_offset = 0x44c,
	.output_sel_mask = 0xf0,
};

static void clk_det_set_output_sel(struct clk_det *clkd, u32 val)
{
	struct regmap *regmap = clkd->os_regmap;
	u32 offset = clkd->desc->output_sel_offset;

	val <<= __ffs(clkd->desc->output_sel_mask);
	mutex_lock(&clk_det_lock);
	regmap_update_bits(regmap, offset, clkd->desc->output_sel_mask, val);
	mutex_unlock(&clk_det_lock);
}

static u32 clk_det_get_output_sel(struct clk_det *clkd)
{
	struct regmap *regmap = clkd->os_regmap;
	u32 offset = clkd->desc->output_sel_offset;
	u32 val;

	mutex_lock(&clk_det_lock);
	regmap_read(regmap, offset, &val);
	mutex_unlock(&clk_det_lock);
	return val >> __ffs(clkd->desc->output_sel_mask);
}


static int available_outputs_show(struct seq_file *s, void *data)
{
	struct clk_det *clkd = s->private;
	int i;

	for (i = 0; i < clkd->desc->output_sel_num; i++) {
		if (!clkd->desc->output_sel[i])
			continue;
		seq_printf(s, "%s%s", i == 0 ? "" : " ", clkd->desc->output_sel[i]);
	}
	if (i)
		seq_puts(s, "\n");
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(available_outputs);

static int output_show(struct seq_file *s, void *data)
{
	struct clk_det *clkd = s->private;
	u32 val = clk_det_get_output_sel(clkd);

	seq_printf(s, "%s\n", clkd->desc->output_sel[val]);
	return 0;
}

static int output_open(struct inode *inode, struct file *file)
{
	return single_open(file, output_show, inode->i_private);
}

#define USER_BUF_PAGE 4096

static ssize_t output_write(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct clk_det *clkd = s->private;
	int i;
	char *buf;

	if (count == 0)
		return 0;
	if (count > USER_BUF_PAGE - 1)
		return -E2BIG;
	buf = memdup_user_nul(userbuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	for (i = 0; i < clkd->desc->output_sel_num; i++) {
		if (!clkd->desc->output_sel[i])
			continue;

		if (sysfs_streq(clkd->desc->output_sel[i], buf)) {
			clk_det_set_output_sel(clkd, i);
			break;
		}
	}

	kfree(buf);
	return i == clkd->desc->output_sel_num ? -EINVAL : count;
}

static const struct file_operations output_fops = {
	.owner		= THIS_MODULE,
	.open		= output_open,
	.read		= seq_read,
	.write		= output_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void clk_det_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct clk_det *clkd = container_of(hw, struct clk_det, hw);

	if (clkd->os_regmap) {
		debugfs_create_file("output", 0644, dentry, clkd, &output_fops);
		debugfs_create_file_unsafe("available_outputs", 0444, dentry, clkd,
					   &available_outputs_fops);
	}
}

static void clk_det_update_bits(struct clk_det *clkd, u32 ofs, u32 mask, u32 val)
{
	u32 reg = readl(clkd->base + ofs);

	reg &= ~mask;
	reg |= (mask & val);
	writel(reg, clkd->base + ofs);
}

static void clk_det_read(struct clk_det *clkd, u32 ofs, u32 *val)
{
	*val = readl(clkd->base + ofs);
}

static unsigned long clk_det_get_freq(struct clk_det *clkd)
{
	const struct clk_det_desc *desc = clkd->desc;
	u32 ctrl_mask;
	u32 val;
	unsigned long freq = 0;
	int ret = 0;

	mutex_lock(&clk_det_lock);

	ctrl_mask = BIT(desc->ctrl_rstn_bit) | BIT(desc->ctrl_cnten_bit);
	clk_det_update_bits(clkd, 0, ctrl_mask, 0);
	clk_det_update_bits(clkd, 0, ctrl_mask, BIT(desc->ctrl_rstn_bit));
	clk_det_update_bits(clkd, 0, ctrl_mask, ctrl_mask);

	if (desc->no_polling_done)
		msleep(10);
	else
		ret = readl_poll_timeout(clkd->base + desc->stat_ofs, val, val & BIT(desc->stat_done_bit),
			0, 100);
	if (!ret) {
		u32 cnt_ofs = desc->separated_cnt_reg ?  desc->cnt_ofs : desc->stat_ofs;

		clk_det_read(clkd, cnt_ofs, &val);
		freq = ((val & desc->cnt_mask) >> desc->cnt_shift) * 100000;
	}

	clk_det_update_bits(clkd, 0, ctrl_mask, 0);

	mutex_unlock(&clk_det_lock);

	return freq;
}

static unsigned long clk_det_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_det *clkd = container_of(hw, struct clk_det, hw);

	if (clkd->ref && !__clk_is_enabled(clkd->ref))
		return 0;

	return clk_det_get_freq(clkd);
}

static const struct clk_ops clk_det_ops = {
	.recalc_rate = clk_det_recalc_rate,
	.debug_init = clk_det_debug_init,
};

struct clk *devm_clk_det_register(struct device *dev, const struct clk_det_initdata *initdata)
{
	struct clk_det *clkd;
	struct clk_init_data clk_initdata = { .name = initdata->name, .ops = &clk_det_ops,
		.flags = CLK_GET_RATE_NOCACHE };
	int ret;
	struct clk_hw *hw;

	clkd = devm_kzalloc(dev, sizeof(*clkd), GFP_KERNEL);
	if (!clkd)
		return ERR_PTR(-ENOMEM);

	clkd->dev = dev;
	clkd->base = initdata->base;
	if (initdata->desc)
		clkd->desc = initdata->desc;
	else {
		if (initdata->type >= ARRAY_SIZE(legacy_descs))
			return ERR_PTR(-EINVAL);
		clkd->desc = &legacy_descs[initdata->type];
	}
	clkd->os_regmap = initdata->os_regmap;
	clkd->ref = initdata->ref;

	hw = &clkd->hw;
	hw->init = &clk_initdata;
	ret = devm_clk_hw_register(dev, hw);
	if (ret) {
		return ERR_PTR(ret);
	}

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_simple_get, hw);
	if (ret) {
		dev_err(dev, "failed to add clk provider: %d\n", ret);
		return ERR_PTR(ret);
	}
	return hw->clk;
}
EXPORT_SYMBOL_GPL(devm_clk_det_register);

static int of_clk_det_get_initdata(struct device *dev, struct clk_det_initdata *data)
{
	struct device_node *np = dev->of_node;
	int ret;

	data->ref = devm_clk_get_optional(dev, 0);
	if (IS_ERR(data->ref))
		return dev_err_probe(dev, PTR_ERR(data->ref), "failed to get clk\n");

	ret = of_property_read_string_index(np, "clock-output-names", 0, &data->name);
	if (ret)
		return ret;

	data->desc = of_device_get_match_data(dev);
	if (!data->desc) {
		ret = of_property_read_u32(np, "realtek,clk-det-type", &data->type);
		if (ret)
			data->type = 0;
	}

	if (data->desc && data->desc->output_sel) {
		data->os_regmap = syscon_regmap_lookup_by_phandle(np, "realtek,os-reg");
		if (IS_ERR(data->os_regmap))
			return PTR_ERR(data->os_regmap);
	}

	return 0;
}

static int clk_det_plat_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct clk_det_initdata initdata = { 0 };
	int ret;
	struct clk *clk;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;
	initdata.base = devm_ioremap(dev, res->start, resource_size(res));
	if (!initdata.base)
		return -ENOMEM;

	ret = of_clk_det_get_initdata(dev, &initdata);
	if (ret)
		return ret;

	clk = devm_clk_det_register(dev, &initdata);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		dev_err(dev, "failed to create clk_det: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id clk_det_match[] = {
	{ .compatible = "realtek,clk-det", },
	{ .compatible = "realtek,rtd1625-clk-det-sc-wrap", .data = &rtd1625_sc_wrap_desc, },
	{ .compatible = "realtek,rtd1635-clk-det-sc-wrap", .data = &rtd1635_sc_wrap_desc, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, clk_det_match);

static struct platform_driver clk_det_driver = {
	.probe = clk_det_plat_probe,
	.driver = {
		.name = "rtk-clk-det",
		.of_match_table = of_match_ptr(clk_det_match),
	},
};
module_platform_driver(clk_det_driver);

MODULE_LICENSE("GPL v2");
