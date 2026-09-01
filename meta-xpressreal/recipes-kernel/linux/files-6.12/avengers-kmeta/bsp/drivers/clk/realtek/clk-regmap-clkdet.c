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
#include <linux/sysfs.h>
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

static int debugfs_available_outputs_show(struct seq_file *s, void *data)
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
DEFINE_SHOW_ATTRIBUTE(debugfs_available_outputs);

static int debugfs_output_show(struct seq_file *s, void *data)
{
	struct clk_regmap_clkdet *clkd = s->private;
	u32 val = clk_regmap_clkdet_get_output_sel(clkd);

	seq_printf(s, "%s\n", clkd->output_sel[val]);
	return 0;
}

static int debugfs_output_open(struct inode *inode, struct file *file)
{
	return single_open(file, debugfs_output_show, inode->i_private);
}

#define USER_BUF_PAGE 4096

static ssize_t debugfs_output_write(struct file *file, const char __user *userbuf,
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

static const struct file_operations debugfs_output_fops = {
	.owner		= THIS_MODULE,
	.open		= debugfs_output_open,
	.read		= seq_read,
	.write          = debugfs_output_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void clk_regmap_clkdet_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct clk_regmap_clkdet *clkd = to_clk_regmap_clkdet(hw);

	if (clkd->output_sel) {
		debugfs_create_file("output", 0644, dentry, clkd, &debugfs_output_fops);
		debugfs_create_file_unsafe("available_outputs", 0444, dentry, clkd,
					   &debugfs_available_outputs_fops);
	}
}

/* Sysfs attributes for output selection */
static ssize_t available_outputs_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	struct clk_regmap_clkdet *clkd = container_of(kobj, struct clk_regmap_clkdet, kobj);
	int i, len = 0;

	if (!clkd->output_sel)
		return 0;

	for (i = 0; i < clkd->n_output_sel; i++) {
		if (!clkd->output_sel[i])
			continue;
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				 i == 0 ? "" : " ", clkd->output_sel[i]);
	}
	if (i)
		len += scnprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;
}

static ssize_t output_show(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	struct clk_regmap_clkdet *clkd = container_of(kobj, struct clk_regmap_clkdet, kobj);
	u32 val;

	if (!clkd->output_sel)
		return -EINVAL;

	val = clk_regmap_clkdet_get_output_sel(clkd);

	return scnprintf(buf, PAGE_SIZE, "%s\n", clkd->output_sel[val]);
}

static ssize_t output_store(struct kobject *kobj,
			    struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	struct clk_regmap_clkdet *clkd = container_of(kobj, struct clk_regmap_clkdet, kobj);
	int i;

	if (!clkd->output_sel)
		return -EINVAL;

	for (i = 0; i < clkd->n_output_sel; i++) {
		if (!clkd->output_sel[i])
			continue;

		if (sysfs_streq(clkd->output_sel[i], buf)) {
			clk_regmap_clkdet_set_output_sel(clkd, i);
			return count;
		}
	}

	return -EINVAL;
}

static ssize_t clk_rate_show(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	struct clk_regmap_clkdet *clkd = container_of(kobj, struct clk_regmap_clkdet, kobj);
	unsigned long rate;

	rate = clk_regmap_clkdet_eval_freq(clkd);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", rate);
}

static struct kobj_attribute dev_attr_available_outputs = __ATTR_RO(available_outputs);
static struct kobj_attribute dev_attr_output = __ATTR_RW(output);
static struct kobj_attribute dev_attr_clk_rate = __ATTR_RO(clk_rate);

static struct attribute *clk_regmap_clkdet_attrs[] = {
	&dev_attr_available_outputs.attr,
	&dev_attr_output.attr,
	&dev_attr_clk_rate.attr,
	NULL,
};

static const struct attribute_group clk_regmap_clkdet_attr_group = {
	.attrs = clk_regmap_clkdet_attrs,
};

static void clk_regmap_clkdet_kobj_release(struct kobject *kobj)
{
	/* empty, clkd is freed differently */
}

static struct kobj_type clk_regmap_clkdet_ktype = {
	.release = clk_regmap_clkdet_kobj_release,
	.sysfs_ops = &kobj_sysfs_ops,
};

static int clk_regmap_clkdet_init(struct clk_hw *hw)
{
	struct clk_regmap_clkdet *clkd = to_clk_regmap_clkdet(hw);
	int ret;
	struct device *dev = clkd->clkr.dev;

	if (clkd->type != CLK_DET_TYPE_GENERIC)
		return -EINVAL;

	if (!clkd->output_sel)
		return 0;

	ret = kobject_init_and_add(&clkd->kobj, &clk_regmap_clkdet_ktype,
				   &dev->kobj, "%s", clk_hw_get_name(hw));
	if (ret) {
		dev_warn(dev, "Failed to create kobject for %s: %d\n",
			 clk_hw_get_name(hw), ret);
		kobject_put(&clkd->kobj);
		return 0;
	}

	ret = sysfs_create_group(&clkd->kobj, &clk_regmap_clkdet_attr_group);
	if (ret) {
		dev_warn(dev, "Failed to create sysfs attributes: %d\n", ret);
		kobject_put(&clkd->kobj);
		return 0;
	}

	clkd->should_put_kobj = 1;
	return 0;
}

static void clk_regmap_clkdet_terminate(struct clk_hw *hw)
{
	struct clk_regmap_clkdet *clkd = to_clk_regmap_clkdet(hw);

	if (clkd->should_put_kobj) {
		sysfs_remove_group(&clkd->kobj, &clk_regmap_clkdet_attr_group);
		kobject_put(&clkd->kobj);
	}
}

const struct clk_ops clk_regmap_clkdet_ops = {
	.recalc_rate = clk_regmap_clkdet_recalc_rate,
	.init = clk_regmap_clkdet_init,
	.terminate = clk_regmap_clkdet_terminate,
	.debug_init = clk_regmap_clkdet_debug_init,
};
EXPORT_SYMBOL_GPL(clk_regmap_clkdet_ops);
