// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include "clk-regmap-clkdet.h"

#define SYS_CLK_DET_REG_FIELD_DET_DONE            BIT(30)
#define SYS_CLK_DET_REG_FIELD_CLK_COUNT           GENMASK(29, 13)
#define SYS_CLK_DET_REG_FIELD_REFCLK_COUNT        GENMASK(12, 2)
#define SYS_CLK_DET_REG_FIELD_ENABLE              BIT(1)
#define SYS_CLK_DET_REG_FIELD_RSTN                BIT(0)

static DEFINE_MUTEX(clk_regmap_clkdet_lock);

static unsigned long clk_regmap_clkdet_eval_freq(struct clk_regmap_clkdet *clkd)
{
	unsigned long freq = 0;
	int ret = 0;
	struct regmap *regmap = clkd->clkr.regmap;
	u32 mask = SYS_CLK_DET_REG_FIELD_ENABLE | SYS_CLK_DET_REG_FIELD_RSTN;
	u32 val;

	mutex_lock(&clk_regmap_clkdet_lock);
	regmap_update_bits(regmap, clkd->ofs, mask, 0);
	regmap_update_bits(regmap, clkd->ofs, mask, 1);
	regmap_update_bits(regmap, clkd->ofs, mask, 3);

	ret = regmap_read_poll_timeout(regmap, clkd->ofs, val,
				       val & SYS_CLK_DET_REG_FIELD_DET_DONE, 0, 100);
	if (!ret) {
		regmap_read(regmap, clkd->ofs, &val);
		freq = FIELD_GET(SYS_CLK_DET_REG_FIELD_CLK_COUNT, val) * 100000;
	}

	regmap_update_bits(regmap, clkd->ofs, mask, 0);

	mutex_unlock(&clk_regmap_clkdet_lock);

	return freq;
}

static void clk_regmap_clkdet_set_output_sel(struct clk_regmap_clkdet *clkd, u32 val)
{
	struct regmap *regmap = clkd->clkr.regmap;
	u32 offset = clkd->reg_output_sel ? clkd->reg_output_sel : clkd->ofs;

	val <<= __ffs(clkd->mask_output_sel);
	mutex_lock(&clk_regmap_clkdet_lock);
	regmap_update_bits(regmap, offset, clkd->mask_output_sel, val);
	mutex_unlock(&clk_regmap_clkdet_lock);
}

static u32 clk_regmap_clkdet_get_output_sel(struct clk_regmap_clkdet *clkd)
{
	struct regmap *regmap = clkd->clkr.regmap;
	u32 offset = clkd->reg_output_sel ? clkd->reg_output_sel : clkd->ofs;
	u32 val;

	mutex_lock(&clk_regmap_clkdet_lock);
	regmap_read(regmap, offset, &val);
	mutex_unlock(&clk_regmap_clkdet_lock);
	return val >> __ffs(clkd->mask_output_sel);
}

static unsigned long clk_regmap_clkdet_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_regmap_clkdet *clkd = to_clk_regmap_clkdet(hw);

	return clk_regmap_clkdet_eval_freq(clkd);
}

static int clk_regmap_clkdet_init(struct clk_hw *hw)
{
	struct clk_regmap_clkdet *clkd = to_clk_regmap_clkdet(hw);

	if (clkd->type != CLK_DET_TYPE_GENERIC)
		return -EINVAL;
	return 0;
}

static int available_outputs_show(struct seq_file *s, void *data)
{
	struct clk_regmap_clkdet *clkd = s->private;
	int i;

	for (i = 0; i < clkd->n_output_sel; i++) {
		if (!clkd->output_sel[i])
			continue;
		seq_printf(s, "%s%s", i == 0 ? "" : " ", clkd->output_sel[i]);
	}
	if (i)
		seq_puts(s, "\n");
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(available_outputs);

static int output_show(struct seq_file *s, void *data)
{
	struct clk_regmap_clkdet *clkd = s->private;
	u32 val = clk_regmap_clkdet_get_output_sel(clkd);

	seq_printf(s, "%s\n", clkd->output_sel[val]);
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
	struct clk_regmap_clkdet *clkd = s->private;
	int i;
	char *buf;

	if (count == 0)
		return 0;
	if (count > USER_BUF_PAGE - 1)
		return -E2BIG;
	buf = memdup_user_nul(userbuf, count);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	for (i = 0; i < clkd->n_output_sel; i++) {
		if (!clkd->output_sel[i])
			continue;

		if (sysfs_streq(clkd->output_sel[i], buf)) {
			clk_regmap_clkdet_set_output_sel(clkd, i);
			break;
		}

	}

	kfree(buf);
	return i == clkd->n_output_sel ? -EINVAL : count;
}

static const struct file_operations output_fops = {
	.owner		= THIS_MODULE,
	.open		= output_open,
	.read		= seq_read,
	.write          = output_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void clk_regmap_clkdet_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct clk_regmap_clkdet *clkd = to_clk_regmap_clkdet(hw);

	if (clkd->output_sel) {
		debugfs_create_file("output", 0644, dentry, clkd, &output_fops);
		debugfs_create_file_unsafe("available_outputs", 0444, dentry, clkd,
					   &available_outputs_fops);
	}
}

const struct clk_ops clk_regmap_clkdet_ops = {
	.recalc_rate = clk_regmap_clkdet_recalc_rate,
	.init = clk_regmap_clkdet_init,
	.debug_init = clk_regmap_clkdet_debug_init,
};
EXPORT_SYMBOL_GPL(clk_regmap_clkdet_ops);
