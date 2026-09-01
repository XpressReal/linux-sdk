// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2025 Realtek Semiconductor Corporation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/iio/buffer.h>
#include <linux/iio/events.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/spinlock.h>
#include <linux/regmap.h>

#define RTK_LSADC_PAD_MAX 4

struct rtk_lsadc_pad_desc {
	u32 pad_reg;
	u32 active_mask;
	u32 adc_val_mask;
	u32 ctrl_reg;
	u32 enable_mask;
	u32 enable_val_on;
	u32 ints_reg;
	u32 inte_reg;
	u32 cmpblk_regs;
	u32 num_cmpblk_regs;
	u32 dma_reg;
};

struct rtk_lsadc_desc {
	u32 num_power_on;
	const struct reg_sequence *power_on;
	u32 num_power_off;
	const struct reg_sequence *power_off;
	u32 max_adc_val;
	u32 max_adc_volt_mv;
	u32 num_pads;
	u32 status_reg;
	struct rtk_lsadc_pad_desc pads[RTK_LSADC_PAD_MAX];
	const unsigned long *available_scan_masks;
	const struct iio_chan_spec *channels;
	u32 num_channels;
	u32 ints_w1c : 1;
};

struct rtk_lsadc_dma_data {
	void *virt;
	dma_addr_t dma_addr;
	size_t dma_size;
	bool enabled;
	ktime_t ppbuf0_ready_ts;
	ktime_t ppbuf1_ready_ts;
};

struct rtk_lsadc_data {
	struct regmap *regmap;
	struct clk *clk;
	struct reset_control *rstc;
	const struct rtk_lsadc_desc *desc;
	spinlock_t lock; // register lock
	struct device *dev;
	u32 num_channels;
	struct iio_chan_spec *channels;
	struct device *pd_phy;
	struct device *pd_dma;
	int irq;
	struct iio_trigger *trig;

	struct {
		u8 buffer[RTK_LSADC_PAD_MAX];

		u64 timestamp __aligned(8);
	} scan;

	wait_queue_head_t wq;
	struct rtk_lsadc_dma_data dmas[RTK_LSADC_PAD_MAX];
};

static u32 rtk_lsadc_reg_read(struct rtk_lsadc_data *lsadc, u32 offset)
{
	u32 val;

	lockdep_assert_held(&lsadc->lock);

	regmap_read(lsadc->regmap, offset, &val);
	return val;
}

static void rtk_lsadc_reg_write(struct rtk_lsadc_data *lsadc, u32 offset, u32 val)
{
	lockdep_assert_held(&lsadc->lock);

	dev_dbg(lsadc->dev, "%s: write %03x: val=%08x\n", __func__, offset, val);
	regmap_write(lsadc->regmap, offset, val);
}

static void rtk_lsadc_reg_update_bits(struct rtk_lsadc_data *lsadc, u32 offset, u32 mask, u32 val)
{
	lockdep_assert_held(&lsadc->lock);

	dev_dbg(lsadc->dev, "%s: update_bits %03x: mask=%08x val=%08x\n", __func__, offset, mask, val);
	regmap_update_bits(lsadc->regmap, offset, mask, val);
}

static void rtk_lsadc_pad_set_cmpblks_inte(struct rtk_lsadc_data *lsadc, u32 pad_id, u32 cmdblk_idx,
					   u32 state)
{
	const struct rtk_lsadc_pad_desc *pad_desc = &lsadc->desc->pads[pad_id];
	u32 mask = 0;

	mask |= BIT(cmdblk_idx + 1);
	rtk_lsadc_reg_update_bits(lsadc, pad_desc->inte_reg, mask, state ? mask : 0);
}

static ssize_t rtk_lsadc_read_cmpblk(struct iio_dev *indio_dev, uintptr_t private,
				     const struct iio_chan_spec *chan, char *buf)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	const struct rtk_lsadc_pad_desc *pad_desc = &lsadc->desc->pads[chan->channel];
	int blk_offset = (int)private;
	u32 val;
	unsigned long flags;

	if (pm_runtime_suspended(lsadc->dev))
		return -EINVAL;

	spin_lock_irqsave(&lsadc->lock, flags);
	val = rtk_lsadc_reg_read(lsadc, pad_desc->cmpblk_regs + blk_offset);
	spin_unlock_irqrestore(&lsadc->lock, flags);

	return snprintf(buf, PAGE_SIZE, "%u:%u\n", (val >> 24), (val >> 16) & 0xff);
}

static ssize_t rtk_lsadc_write_cmpblk(struct iio_dev *indio_dev, uintptr_t private,
				      const struct iio_chan_spec *chan, const char *buf,
				      size_t len)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	const struct rtk_lsadc_pad_desc *pad_desc = &lsadc->desc->pads[chan->channel];
	int blk_offset = (int)private;
	u32 val;
	u32 h, l;
	unsigned long flags;

	if (pm_runtime_suspended(lsadc->dev))
		return -EINVAL;

	if (sscanf(buf, "%u:%u", &h, &l) != 2)
		return -EINVAL;
	if (h > 255 || l > 255 || l > h)
		return -EINVAL;

	val = (h << 24) | (l << 16) | (h == 0 ? 0 : BIT(15));

	spin_lock_irqsave(&lsadc->lock, flags);
	rtk_lsadc_reg_write(lsadc, pad_desc->cmpblk_regs + blk_offset, val);
	rtk_lsadc_pad_set_cmpblks_inte(lsadc, chan->channel, blk_offset / 4, h == 0 ? 0 : 1);
	spin_unlock_irqrestore(&lsadc->lock, flags);

	return len;
}

#define RTD1625_LSADC0_COMPAREBLK(_n) \
{ \
	.name = "cmpblk" # _n "_raw", \
	.read = rtk_lsadc_read_cmpblk, \
	.write = rtk_lsadc_write_cmpblk, \
	.shared = IIO_SEPARATE, \
	.private = (uintptr_t)((_n) * 4), \
}

static int rtk_lsadc_dma_start(struct rtk_lsadc_data *lsadc, u32 pad_id)
{
	struct rtk_lsadc_dma_data *dd = &lsadc->dmas[pad_id];
	const struct rtk_lsadc_pad_desc *pad_desc = &lsadc->desc->pads[pad_id];
	unsigned long flags;
	int ret;

	if (dd->enabled)
		return -EBUSY;

	ret = pm_runtime_resume_and_get(lsadc->dev);
	if (ret)
		return ret;

	dd->dma_size = PAGE_SIZE;
	dd->virt = dma_alloc_coherent(lsadc->dev, dd->dma_size, &dd->dma_addr, GFP_KERNEL);
	if (!dd->virt) {
		pm_runtime_mark_last_busy(lsadc->dev);
		pm_runtime_put_autosuspend(lsadc->dev);
		return -ENOMEM;
	}

	dd->enabled = true;
	dd->ppbuf0_ready_ts = 0;
	dd->ppbuf1_ready_ts = 0;

	spin_lock_irqsave(&lsadc->lock, flags);
	rtk_lsadc_reg_write(lsadc, pad_desc->dma_reg + 4, dd->dma_addr >> 3);
	rtk_lsadc_reg_update_bits(lsadc, pad_desc->dma_reg, 0x3f, 9);
	rtk_lsadc_reg_update_bits(lsadc, pad_desc->inte_reg, 0x1800, 0x1800);
	spin_unlock_irqrestore(&lsadc->lock, flags);

	return 0;
}

static int rtk_lsadc_dma_stop(struct rtk_lsadc_data *lsadc, u32 pad_id)
{
	struct rtk_lsadc_dma_data *dd = &lsadc->dmas[pad_id];
	const struct rtk_lsadc_pad_desc *pad_desc = &lsadc->desc->pads[pad_id];
	unsigned long flags;

	if (!dd->enabled)
		return 0;

	spin_lock_irqsave(&lsadc->lock, flags);
	rtk_lsadc_reg_update_bits(lsadc, pad_desc->inte_reg, 0x1800, 0);
	rtk_lsadc_reg_write(lsadc, pad_desc->dma_reg + 4, 0);
	rtk_lsadc_reg_update_bits(lsadc, pad_desc->dma_reg, 0x3f, 0);
	spin_unlock_irqrestore(&lsadc->lock, flags);

	dma_free_coherent(lsadc->dev, dd->dma_size, dd->virt, dd->dma_addr);

	pm_runtime_mark_last_busy(lsadc->dev);
	pm_runtime_put_autosuspend(lsadc->dev);
	dd->enabled = false;
	return 0;
}

static int rtk_lsadc_dma_wait_next_ppbuf(struct rtk_lsadc_data *lsadc, u32 pad_id)
{
	struct rtk_lsadc_dma_data *dd = &lsadc->dmas[pad_id];
	ktime_t ppbuf0_ready_ts = dd->ppbuf0_ready_ts, ppbuf1_ready_ts = dd->ppbuf1_ready_ts;

	return wait_event_interruptible(lsadc->wq,
		ppbuf0_ready_ts != dd->ppbuf0_ready_ts || ppbuf1_ready_ts != dd->ppbuf1_ready_ts);
}

static ssize_t rtk_lsadc_write_dma(struct iio_dev *indio_dev, uintptr_t private,
				   const struct iio_chan_spec *chan, const char *buf,
				   size_t len)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	int ret = -EINVAL;

	if (sysfs_streq("start", buf))
		ret = rtk_lsadc_dma_start(lsadc, chan->channel);
	else if (sysfs_streq("stop", buf))
		ret = rtk_lsadc_dma_stop(lsadc, chan->channel);
	else if (sysfs_streq("wait", buf))
		ret = rtk_lsadc_dma_wait_next_ppbuf(lsadc, chan->channel);

	return ret ?: len;
}

static int rtk_lsadc_dma_status(struct rtk_lsadc_data *lsadc, u32 pad_id)
{
	struct rtk_lsadc_dma_data *dd = &lsadc->dmas[pad_id];

	if (!dd->enabled)
		return -EINVAL;
	if (dd->ppbuf0_ready_ts == dd->ppbuf1_ready_ts)
		return -EBUSY;
	return !(dd->ppbuf0_ready_ts > dd->ppbuf1_ready_ts);
}

static ssize_t rtk_lsadc_read_dma(struct iio_dev *indio_dev, uintptr_t private,
				  const struct iio_chan_spec *chan, char *buf)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", rtk_lsadc_dma_status(lsadc, chan->channel));
}

static ssize_t rtk_lsadc_dma_data_read(struct rtk_lsadc_dma_data *dd, char *buf, loff_t ppos,
				       size_t count)
{
	if (!dd->enabled)
		return -EINVAL;

	if (ppos == 0 && count == (dd->dma_size / 2))
		memcpy(buf, dd->virt, count);
	else if (ppos == 1 && count == (dd->dma_size / 2))
		memcpy(buf, dd->virt + dd->dma_size / 2, count);
	else
		return -EINVAL;
	return count;
}

#define LSADC_DMA_BIN_ATTR(n) \
static ssize_t in_voltage ## n ## _dma_data_read(struct file *filp, struct kobject *kobj, \
					       struct bin_attribute *attr, char *buf, loff_t ppos, \
					       size_t count) \
{ \
	struct iio_dev *indio_dev = dev_to_iio_dev(kobj_to_dev(kobj)); \
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev); \
\
	return rtk_lsadc_dma_data_read(&lsadc->dmas[n], buf, ppos, count); \
} \
static BIN_ATTR(in_voltage ## n ## _dma_data, 0444, in_voltage ## n ## _dma_data_read, NULL, 0)

LSADC_DMA_BIN_ATTR(0);
LSADC_DMA_BIN_ATTR(1);
LSADC_DMA_BIN_ATTR(2);
LSADC_DMA_BIN_ATTR(3);

static struct bin_attribute *rtk_lsadc_bin_attrs[] = {
	&bin_attr_in_voltage0_dma_data,
	&bin_attr_in_voltage1_dma_data,
	&bin_attr_in_voltage2_dma_data,
	&bin_attr_in_voltage3_dma_data,
	NULL,
};

static umode_t rtk_lsadc_is_bin_visible(struct kobject *kobj, struct bin_attribute *attr,
					int n)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(kobj_to_dev(kobj));
        struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);

	if (attr == &bin_attr_in_voltage1_dma_data && lsadc->desc->num_pads < 2)
		return 0;
	if (attr == &bin_attr_in_voltage2_dma_data && lsadc->desc->num_pads < 3)
		return 0;
	if (attr == &bin_attr_in_voltage3_dma_data && lsadc->desc->num_pads < 4)
		return 0;
	return attr->attr.mode;
}

static const struct attribute_group rtk_lsadc_attr_group = {
	.bin_attrs = rtk_lsadc_bin_attrs,
	.is_bin_visible = rtk_lsadc_is_bin_visible,
};

#define LSADC0_PAD0             0x000
#define LSADC0_PAD0_DMA_CTRL1   0x010
#define LSADC0_PAD0_DMA_CTRL2   0x014
#define LSADC0_PAD0_ADC_DATA    0x030
#define LSADC0_PAD0_CTRL        0x040
#define LSADC0_STATUS           0x050
#define LSADC0_ANALOG_CTRL      0x054
#define LSADC0_PAD0_LEVEL_SET_0 0x05c
#define LSADC0_POWER            0x0ec
#define LSADC0_PAD0_INTS        0x0f0
#define LSADC0_PAD0_INTE        0x100

#define RTD1625_LSADC0_DESC_PAD(_n)    \
{ \
	.pad_reg = LSADC0_PAD0 + (_n) * 4, \
	.active_mask = BIT(31), \
	.adc_val_mask = 0xff, \
	.ctrl_reg = LSADC0_PAD0_CTRL + (_n) * 4, \
	.enable_mask = 0x1, \
	.enable_val_on = 0x1, \
	.ints_reg = LSADC0_PAD0_INTS + (_n) * 4, \
	.inte_reg = LSADC0_PAD0_INTE + (_n) * 4, \
	.cmpblk_regs = LSADC0_PAD0_LEVEL_SET_0 + (_n) * 32, \
	.num_cmpblk_regs = 8, \
	.dma_reg = LSADC0_PAD0_DMA_CTRL1 + (_n) * 8, \
}

static const struct iio_chan_spec_ext_info rtd1625_lsadc0_ext_info[] = {
	RTD1625_LSADC0_COMPAREBLK(0),
	RTD1625_LSADC0_COMPAREBLK(1),
	RTD1625_LSADC0_COMPAREBLK(2),
	RTD1625_LSADC0_COMPAREBLK(3),
	RTD1625_LSADC0_COMPAREBLK(4),
	RTD1625_LSADC0_COMPAREBLK(5),
	RTD1625_LSADC0_COMPAREBLK(6),
	RTD1625_LSADC0_COMPAREBLK(7),
	{
		.name = "dma",
		.read = rtk_lsadc_read_dma,
		.write = rtk_lsadc_write_dma,
		.shared = IIO_SEPARATE,
	},
	{}
};

#define RTD1625_LSADC0_CHAN(index) \
{ \
	.type = IIO_VOLTAGE, \
	.indexed = 1, \
	.channel = (index), \
	.address = (index), \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) , \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
	.scan_type = { .sign = 'u', .realbits = 8, .storagebits = 8, .shift = 0,}, \
	.scan_index = (index), \
	.ext_info = rtd1625_lsadc0_ext_info, \
}

static const struct iio_chan_spec rtd1625_lsadc0_channels[] = {
	RTD1625_LSADC0_CHAN(0),
	RTD1625_LSADC0_CHAN(1),
	RTD1625_LSADC0_CHAN(2),
	RTD1625_LSADC0_CHAN(3),
};

static const unsigned long rtd1625_lsadc0_available_scan_masks[] = {
	0xf, 0x0
};

static const struct reg_sequence rtd1625_lsadc0_power_on[] = {
	{ LSADC0_POWER, 0x0000003f },
	{ LSADC0_ANALOG_CTRL, 0x00000043, 200 },
};

static const struct reg_sequence rtd1625_lsadc0_power_off[] = {
	{ LSADC0_ANALOG_CTRL, 0x00000042 },
};

static const struct rtk_lsadc_desc rtd1625_lsadc0_desc = {
	.num_power_on = ARRAY_SIZE(rtd1625_lsadc0_power_on),
	.power_on = rtd1625_lsadc0_power_on,
	.num_power_off = ARRAY_SIZE(rtd1625_lsadc0_power_off),
	.power_off = rtd1625_lsadc0_power_off,
	.max_adc_val = 255,
	.max_adc_volt_mv = 1800,
	.num_pads = 4,
	.status_reg = LSADC0_STATUS,
	.pads[0] = RTD1625_LSADC0_DESC_PAD(0),
	.pads[1] = RTD1625_LSADC0_DESC_PAD(1),
	.pads[2] = RTD1625_LSADC0_DESC_PAD(2),
	.pads[3] = RTD1625_LSADC0_DESC_PAD(3),
	.channels = rtd1625_lsadc0_channels,
	.num_channels = ARRAY_SIZE(rtd1625_lsadc0_channels),
	.available_scan_masks = rtd1625_lsadc0_available_scan_masks,
};

static const struct iio_chan_spec_ext_info rtd1635_lsadc2_ext_info[] = {
	RTD1625_LSADC0_COMPAREBLK(0),
	RTD1625_LSADC0_COMPAREBLK(1),
	RTD1625_LSADC0_COMPAREBLK(2),
	RTD1625_LSADC0_COMPAREBLK(3),
	RTD1625_LSADC0_COMPAREBLK(4),
	RTD1625_LSADC0_COMPAREBLK(5),
	{
		.name = "dma",
		.read = rtk_lsadc_read_dma,
		.write = rtk_lsadc_write_dma,
		.shared = IIO_SEPARATE,
	},
	{}
};

/*
 * RTD1635_LSADC1: 0x98140a00
 * RTD1635_LSADC2: 0x98140e00
 */
#define RTD1635_LSADCX_PAD0             0x000
#define RTD1635_LSADCX_PAD0_DMA_CTRL1   0x010
#define RTD1635_LSADCX_PAD0_DMA_CTRL2   0x014
#define RTD1635_LSADCX_PAD0_ADC_DATA    0x030
#define RTD1635_LSADCX_PAD0_CTRL        0x040
#define RTD1635_LSADCX_STATUS           0x050
#define RTD1635_LSADCX_ANALOG_CTRL      0x054
#define RTD1635_LSADCX_PAD0_LEVEL_SET_0 0x05c
#define RTD1635_LSADCX_POWER            0x0d0
#define RTD1635_LSADCX_PAD0_INTS        0x0d4
#define RTD1635_LSADCX_PAD0_INTE        0x0d8

#define RTD1635_LSADC1_ANALOG_CTRL_0    0x054
#define RTD1635_LSADC1_ANALOG_CTRL_1    0x058

#define RTD1635_LSADCX_DESC_PAD0() \
{ \
	.pad_reg = RTD1635_LSADCX_PAD0, \
	.active_mask = BIT(31), \
	.adc_val_mask = 0xff, \
	.ctrl_reg = RTD1635_LSADCX_PAD0_CTRL, \
	.enable_mask = 0x1, \
	.enable_val_on = 0x1, \
	.ints_reg = RTD1635_LSADCX_PAD0_INTS, \
	.inte_reg = RTD1635_LSADCX_PAD0_INTE, \
	.cmpblk_regs = RTD1635_LSADCX_PAD0_LEVEL_SET_0, \
	.num_cmpblk_regs = 6, \
	.dma_reg = RTD1635_LSADCX_PAD0_DMA_CTRL1, \
}

#define RTD1635_LSADCX_CHAN(index) \
{ \
	.type = IIO_VOLTAGE, \
	.indexed = 1, \
	.channel = (index), \
	.address = (index), \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) , \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
	.scan_type = { .sign = 'u', .realbits = 8, .storagebits = 8, .shift = 0,}, \
	.scan_index = (index), \
	.ext_info = rtd1635_lsadc2_ext_info, \
}

static const struct iio_chan_spec rtd1635_lsadc0_channels[] = {
	RTD1625_LSADC0_CHAN(0),
	RTD1625_LSADC0_CHAN(1),
	RTD1625_LSADC0_CHAN(2),
};

static const unsigned long rtd1635_lsadc0_available_scan_masks[] = {
	0x7, 0x0
};

static const struct rtk_lsadc_desc rtd1635_lsadc0_desc = {
	.num_power_on = ARRAY_SIZE(rtd1625_lsadc0_power_on),
	.power_on = rtd1625_lsadc0_power_on,
	.num_power_off = ARRAY_SIZE(rtd1625_lsadc0_power_off),
	.power_off = rtd1625_lsadc0_power_off,
	.max_adc_val = 255,
	.max_adc_volt_mv = 1800,
	.num_pads = 3,
	.status_reg = LSADC0_STATUS,
	.pads[0] = RTD1625_LSADC0_DESC_PAD(0),
	.pads[1] = RTD1625_LSADC0_DESC_PAD(1),
	.pads[2] = RTD1625_LSADC0_DESC_PAD(2),
	.channels = rtd1635_lsadc0_channels,
	.num_channels = ARRAY_SIZE(rtd1635_lsadc0_channels),
	.available_scan_masks = rtd1635_lsadc0_available_scan_masks,
	.ints_w1c = 1,
};

static const struct iio_chan_spec rtd1635_lsadcx_channels[] = {
	RTD1635_LSADCX_CHAN(0),
};

static const unsigned long rtd1635_lsadcx_available_scan_masks[] = {
	0x1, 0x0
};

static const struct reg_sequence rtd1635_lsadc1_power_on[] = {
	{ RTD1635_LSADCX_POWER, 0x0000003f },
	{ RTD1635_LSADC1_ANALOG_CTRL_0, 0x8c036462, 200 },
};

static const struct reg_sequence rtd1635_lsadc1_power_off[] = {
	{ RTD1635_LSADC1_ANALOG_CTRL_0, 0x0c036462 },
};

static const struct rtk_lsadc_desc rtd1635_lsadc1_desc = {
	.num_power_on = ARRAY_SIZE(rtd1635_lsadc1_power_on),
	.power_on = rtd1635_lsadc1_power_on,
	.num_power_off = ARRAY_SIZE(rtd1635_lsadc1_power_off),
	.power_off = rtd1635_lsadc1_power_off,
	.max_adc_val = 255,
	.max_adc_volt_mv = 1200,
	.num_pads = 1,
	.status_reg = RTD1635_LSADCX_STATUS,
	.pads[0] = RTD1635_LSADCX_DESC_PAD0(),
	.channels = rtd1635_lsadcx_channels,
	.num_channels = ARRAY_SIZE(rtd1635_lsadcx_channels),
	.available_scan_masks = rtd1635_lsadcx_available_scan_masks,
	.ints_w1c = 1,
};

static const struct reg_sequence rtd1635_lsadc2_power_on[] = {
	{ RTD1635_LSADCX_POWER, 0x0000003f },
	{ RTD1635_LSADCX_ANALOG_CTRL, 0x00000043, 200 },
};

static const struct reg_sequence rtd1635_lsadc2_power_off[] = {
	{ RTD1635_LSADCX_ANALOG_CTRL, 0x00000042 },
};

static const struct rtk_lsadc_desc rtd1635_lsadc2_desc = {
	.num_power_on = ARRAY_SIZE(rtd1635_lsadc2_power_on),
	.power_on = rtd1635_lsadc2_power_on,
	.num_power_off = ARRAY_SIZE(rtd1635_lsadc2_power_off),
	.power_off = rtd1635_lsadc2_power_off,
	.max_adc_val = 255,
	.max_adc_volt_mv = 1800,
	.num_pads = 1,
	.status_reg = RTD1635_LSADCX_STATUS,
	.pads[0] = RTD1635_LSADCX_DESC_PAD0(),
	.channels = rtd1635_lsadcx_channels,
	.num_channels = ARRAY_SIZE(rtd1635_lsadcx_channels),
	.available_scan_masks = rtd1635_lsadcx_available_scan_masks,
	.ints_w1c = 1,
};

static inline u32 rtk_lsadc_pad_get_adc_val(struct rtk_lsadc_data *lsadc, u32 pad_id)
{
	const struct rtk_lsadc_pad_desc *pad_desc = &lsadc->desc->pads[pad_id];
	u32 val;

	val = rtk_lsadc_reg_read(lsadc, pad_desc->pad_reg);
	return val & pad_desc->adc_val_mask;
}

static inline u32 rtk_lsadc_pad_get_active(struct rtk_lsadc_data *lsadc, u32 pad_id)
{
	const struct rtk_lsadc_pad_desc *pad_desc = &lsadc->desc->pads[pad_id];
	u32 val;

	val = rtk_lsadc_reg_read(lsadc, pad_desc->pad_reg);
	return val >> 31;
}

static inline void rtk_lsadc_pad_set_active(struct rtk_lsadc_data *lsadc, u32 pad_id, u32 active)
{
	const struct rtk_lsadc_pad_desc *pad_desc = &lsadc->desc->pads[pad_id];

	rtk_lsadc_reg_update_bits(lsadc, pad_desc->pad_reg, BIT(31),
				  active ? BIT(31) : 0);
}

static irqreturn_t rtk_lsadc_interrupt_handler(int irq, void *data)
{
	struct iio_dev *indio_dev = data;
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	const struct rtk_lsadc_desc *desc = lsadc->desc;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&lsadc->lock, flags);
	for (i = 0; i < indio_dev->num_channels; i++) {
		u32 ints = rtk_lsadc_reg_read(lsadc, desc->pads[i].ints_reg);
		u32 inte = rtk_lsadc_reg_read(lsadc, desc->pads[i].inte_reg);

		if ((ints & inte) == 0)
			continue;

		if (ints & BIT(12))
			lsadc->dmas[i].ppbuf1_ready_ts = ktime_get();
		else if (ints & BIT(11))
			lsadc->dmas[i].ppbuf0_ready_ts = ktime_get();

		rtk_lsadc_reg_write(lsadc, desc->pads[i].ints_reg, desc->ints_w1c ? ~0 : 0);
	}
	spin_unlock_irqrestore(&lsadc->lock, flags);

	if (iio_buffer_enabled(indio_dev))
		iio_trigger_poll(indio_dev->trig);

	wake_up(&lsadc->wq);

	return IRQ_HANDLED;
}

static irqreturn_t rtk_lsadc_trigger_handler(int irq, void *data)
{
	struct iio_poll_func *pf = data;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	unsigned long flags;
	int i, j = 0;

	spin_lock_irqsave(&lsadc->lock, flags);
	for (i = 0; i < indio_dev->num_channels; i++)
		if (test_bit(i, indio_dev->active_scan_mask))
			lsadc->scan.buffer[j++] = rtk_lsadc_pad_get_adc_val(lsadc, i);
	spin_unlock_irqrestore(&lsadc->lock, flags);

	iio_push_to_buffers_with_timestamp(indio_dev, &lsadc->scan, iio_get_time_ns(indio_dev));
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int rtk_lsadc_set_trigger_state(struct iio_trigger *trig, bool state)
{
	return 0;
}

static const struct iio_trigger_ops rtk_lsadc_trigger_ops = {
	.set_trigger_state = rtk_lsadc_set_trigger_state,
};

static int rtk_lsadc_buffer_preenable(struct iio_dev *indio_dev)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);

	return pm_runtime_resume_and_get(lsadc->dev);
}

static int rtk_lsadc_buffer_postdisable(struct iio_dev *indio_dev)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);

	pm_runtime_mark_last_busy(lsadc->dev);
	pm_runtime_put_autosuspend(lsadc->dev);
	return 0;
}

static const struct iio_buffer_setup_ops rtk_lsadc_buffer_setup_ops = {
	.preenable =  rtk_lsadc_buffer_preenable,
	.postdisable =  rtk_lsadc_buffer_postdisable,
};

static int rtk_lsadc_read_channel(struct rtk_lsadc_data *lsadc, int x, int *val)
{
	int ret;
	unsigned long flags;

	ret = pm_runtime_resume_and_get(lsadc->dev);
	if (ret)
		return ret;

	spin_lock_irqsave(&lsadc->lock, flags);
	*val = rtk_lsadc_pad_get_adc_val(lsadc, x);
	spin_unlock_irqrestore(&lsadc->lock, flags);

	pm_runtime_mark_last_busy(lsadc->dev);
	pm_runtime_put_autosuspend(lsadc->dev);
	return 0;
}
static int rtk_lsadc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
			      int *val, int *val2, long mask)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	const struct rtk_lsadc_desc *desc = lsadc->desc;
	int x = chan->channel;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = rtk_lsadc_read_channel(lsadc, x, val);
		return ret ?: IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		*val = desc->max_adc_volt_mv;
		*val2 = desc->max_adc_val;
		return IIO_VAL_FRACTIONAL;

	default:
		return -EINVAL;
	}
}

static const struct iio_info rtk_lsadc_iio_info = {
	.read_raw  = &rtk_lsadc_read_raw,
};

static int rtk_lsadc_runtime_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	const struct rtk_lsadc_desc *desc = lsadc->desc;
	int i;
	unsigned long flags;

	pm_runtime_get_sync(lsadc->pd_phy);
	pm_runtime_get_sync(lsadc->pd_dma);
	reset_control_deassert(lsadc->rstc);
	clk_prepare_enable(lsadc->clk);

	spin_lock_irqsave(&lsadc->lock, flags);
	for (i = 0; i < indio_dev->num_channels; i++)
		rtk_lsadc_reg_write(lsadc, desc->pads[i].pad_reg, 0x80000100);

	if (desc->num_power_on)
		regmap_multi_reg_write(lsadc->regmap, desc->power_on, desc->num_power_on);

	for (i = 0; i < indio_dev->num_channels; i++) {
		rtk_lsadc_reg_write(lsadc, desc->pads[i].ctrl_reg, 0x13f90001);
		udelay(10);
	}
	spin_unlock_irqrestore(&lsadc->lock, flags);

	return 0;
}

static int rtk_lsadc_runtime_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	const struct rtk_lsadc_desc *desc = lsadc->desc;

	if (desc->num_power_off)
		regmap_multi_reg_write(lsadc->regmap, desc->power_off, desc->num_power_off);

	clk_disable_unprepare(lsadc->clk);
	reset_control_assert(lsadc->rstc);
	pm_runtime_put(lsadc->pd_phy);
	pm_runtime_put(lsadc->pd_dma);
	return 0;
}

static const struct dev_pm_ops rtk_lsadc_pm_ops = {
	SET_RUNTIME_PM_OPS(rtk_lsadc_runtime_suspend,
			   rtk_lsadc_runtime_resume,
			   NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static void rtk_lsadc_detach_pds(void *data)
{
	struct rtk_lsadc_data *lsadc = data;

	dev_pm_domain_detach(lsadc->pd_dma, 0);
	dev_pm_domain_detach(lsadc->pd_phy, 0);
}

static const struct regmap_config rtk_lsadc_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.fast_io = true,
	.disable_locking = true,
};

static int rtk_lsadc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct iio_dev *indio_dev;
	struct rtk_lsadc_data *lsadc;
	void __iomem *base;
	int ret = -EINVAL;

	indio_dev = devm_iio_device_alloc(dev, sizeof(struct rtk_lsadc_data));
	if (!indio_dev)
		return -ENOMEM;

	lsadc = iio_priv(indio_dev);
	lsadc->dev = dev;
	spin_lock_init(&lsadc->lock);
	init_waitqueue_head(&lsadc->wq);
	platform_set_drvdata(pdev, indio_dev);

	lsadc->desc = of_device_get_match_data(dev);
	if (!lsadc->desc) {
		dev_err(dev, "failed to get match data\n");
		return -EINVAL;
	}

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	lsadc->regmap = devm_regmap_init_mmio(dev, base, &rtk_lsadc_regmap_config);
	if (IS_ERR(lsadc->regmap))
		return PTR_ERR(lsadc->regmap);

	lsadc->pd_phy = dev_pm_domain_attach_by_name(dev, "phy");
	if (IS_ERR_OR_NULL(lsadc->pd_phy)) {
		ret = !lsadc->pd_phy ? -ENODEV : PTR_ERR(lsadc->pd_phy);
		return dev_err_probe(dev, ret, "failed to get pd_phy\n");
	}

	lsadc->pd_dma = dev_pm_domain_attach_by_name(dev, "dma");
	if (IS_ERR_OR_NULL(lsadc->pd_dma)) {
		dev_pm_domain_detach(lsadc->pd_phy, 0);
		ret = !lsadc->pd_dma ? -ENODEV : PTR_ERR(lsadc->pd_dma);
		return dev_err_probe(dev, ret, "failed to get pd_dma\n");
	}

	ret = devm_add_action_or_reset(dev, rtk_lsadc_detach_pds, lsadc);
	if (ret) {
		dev_pm_domain_detach(lsadc->pd_dma, 0);
		dev_pm_domain_detach(lsadc->pd_phy, 0);
		return ret;
	}

	lsadc->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(lsadc->clk))
		return dev_err_probe(dev, PTR_ERR(lsadc->clk), "failed to get clk\n");

	lsadc->rstc = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(lsadc->rstc))
		return dev_err_probe(dev, PTR_ERR(lsadc->rstc), "failed to get reset control\n");

	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to get irq\n");
	lsadc->irq = ret;

	indio_dev->dev.parent = dev;
	indio_dev->dev.of_node = dev->of_node;
	indio_dev->name = dev_name(dev);
	indio_dev->info = &rtk_lsadc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->available_scan_masks = lsadc->desc->available_scan_masks;
	indio_dev->channels = lsadc->desc->channels;
	indio_dev->num_channels = lsadc->desc->num_channels;

	ret = devm_iio_triggered_buffer_setup(dev, indio_dev, iio_pollfunc_store_time,
					      rtk_lsadc_trigger_handler, &rtk_lsadc_buffer_setup_ops);
	if (ret)
		return dev_err_probe(dev, ret, "failed to setup iio_triggered_buffer\n");

	lsadc->trig = devm_iio_trigger_alloc(dev, "%s-dev%d", indio_dev->name, 0);
	if (!lsadc->trig)
		return dev_err_probe(dev, ret, "failed to allocate iio_trigger\n");

	lsadc->trig->ops = &rtk_lsadc_trigger_ops;
	lsadc->trig->dev.parent = dev;
	iio_trigger_set_drvdata(lsadc->trig, indio_dev);
	ret = devm_iio_trigger_register(dev, lsadc->trig);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register iio_trigger\n");

	ret = devm_iio_device_register(dev, indio_dev);
	if (ret) {
		dev_err(dev, "failed to register iio device\n");
		return ret;
	}

	indio_dev->trig = iio_trigger_get(lsadc->trig);

	ret = devm_request_irq(dev, lsadc->irq, rtk_lsadc_interrupt_handler,
			       IRQF_ONESHOT, dev_name(dev), indio_dev);
	if (ret < 0) {
		dev_err(dev, "unable to request irq#%d: %d\n", lsadc->irq, ret);
		return ret;
	}

	ret = sysfs_create_group(&indio_dev->dev.kobj, &rtk_lsadc_attr_group);
	if (ret) {
		dev_err(dev, "failed to create attrs\n");
		return ret;
	}

	pm_runtime_set_suspended(dev);
	pm_runtime_set_autosuspend_delay(dev, 20);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(dev);
	return 0;
}

static void rtk_lsadc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	iio_trigger_put(indio_dev->trig);
}

static void rtk_lsadc_shutdown(struct platform_device *pdev)
{
	pm_runtime_force_suspend(&pdev->dev);
}

static const struct of_device_id rtk_lsadc_of_match[] = {
	{ .compatible = "realtek,rtd1625-lsadc0", .data = &rtd1625_lsadc0_desc, },
	{ .compatible = "realtek,rtd1635-lsadc0", .data = &rtd1635_lsadc0_desc, },
	{ .compatible = "realtek,rtd1635-lsadc1", .data = &rtd1635_lsadc1_desc, },
	{ .compatible = "realtek,rtd1635-lsadc2", .data = &rtd1635_lsadc2_desc, },
	{}
};
MODULE_DEVICE_TABLE(of, rtk_lsadc_of_match);

static struct platform_driver rtk_lsadc_platform_driver = {
	.driver		= {
		.owner	        = THIS_MODULE,
		.name	        = "rtk-rtd1625-lsadc",
		.pm	        = &rtk_lsadc_pm_ops,
		.of_match_table = rtk_lsadc_of_match,
		.probe_type     = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe		= rtk_lsadc_probe,
	.remove		= rtk_lsadc_remove,
	.shutdown	= rtk_lsadc_shutdown,
};
module_platform_driver(rtk_lsadc_platform_driver);

MODULE_DESCRIPTION("RTK RTD1625 LSADC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtk-lsadc");
