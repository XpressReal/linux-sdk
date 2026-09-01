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
	u32 power_reg;
	u32 power_mask;
	u32 power_val_on;
	u32 max_adc_val;
	u32 analog_reg;
	u32 jd_power_mask;
	u32 jd_power_val_on;
	u32 max_adc_volt_mv;
	u32 num_pads;
	struct rtk_lsadc_pad_desc pads[RTK_LSADC_PAD_MAX];
	const struct iio_chan_spec_ext_info *ext_info;
	const unsigned long *available_scan_masks;
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
	void __iomem *base;
	struct clk *clk;
	struct reset_control *rstc;
	const struct rtk_lsadc_desc *desc;
	spinlock_t lock; // register lock
	struct device *dev;
	u32 num_channels;
	struct iio_chan_spec *channels;
	struct device *pd_phy;
	struct device *pd_dma;
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
	return readl(lsadc->base + offset);
}

static void rtk_lsadc_reg_write(struct rtk_lsadc_data *lsadc, u32 offset, u32 val)
{
	dev_dbg(lsadc->dev, "%s: write %03x: %08x\n", __func__, offset, val);
	writel(val, lsadc->base + offset);
}

static void rtk_lsadc_reg_update_bits(struct rtk_lsadc_data *lsadc, u32 offset, u32 mask, u32 val)
{
	u32 v;

	v = rtk_lsadc_reg_read(lsadc, offset) & ~mask;
	v |= (mask & val);
	rtk_lsadc_reg_write(lsadc, offset, v);
}

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

static ssize_t rtk_lsadc_read_cmpblk(struct iio_dev *indio_dev, uintptr_t private,
				     const struct iio_chan_spec *chan, char *buf)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	const struct rtk_lsadc_pad_desc *pad_desc = &lsadc->desc->pads[chan->channel];
	int blk_offset = (int)private;
	u32 val;

	val = rtk_lsadc_reg_read(lsadc, pad_desc->cmpblk_regs + blk_offset);

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

	if (sscanf(buf, "%u:%u", &h, &l) != 2)
		return -EINVAL;
	if (h > 255 || l > 255 || l > h)
		return -EINVAL;

	val = (h << 24) | (l << 16) | (h == 0 ? 0 : BIT(15));
	rtk_lsadc_reg_write(lsadc, pad_desc->cmpblk_regs + blk_offset, val);

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

	if (dd->enabled)
		return -EBUSY;

	dd->dma_size = PAGE_SIZE;
	dd->virt = dma_alloc_coherent(lsadc->dev, dd->dma_size, &dd->dma_addr, GFP_KERNEL);
	if (!dd->virt)
		return -ENOMEM;

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
	struct iio_dev *indio_dev = dev_get_drvdata(kobj_to_dev(kobj)); \
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

static const struct attribute_group rtk_lsadc_attr_group = {
	.bin_attrs = rtk_lsadc_bin_attrs,
};

static const struct iio_chan_spec_ext_info rtd1625_ext_info[] = {
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

static const unsigned long rtd1625_lsadc0_available_scan_masks[] = {
	0xf, 0x0
};

static const struct rtk_lsadc_desc rtd1625_lsadc0_desc = {
	.power_reg = LSADC0_POWER,
	.power_mask = 0x3f,
	.power_val_on = 0x1f,
	.analog_reg = LSADC0_ANALOG_CTRL,
	.jd_power_mask = 0x1,
	.jd_power_val_on = 0x1,
	.max_adc_val = 255,
	.max_adc_volt_mv = 1800,
	.num_pads = 4,
	.pads[0] = RTD1625_LSADC0_DESC_PAD(0),
	.pads[1] = RTD1625_LSADC0_DESC_PAD(1),
	.pads[2] = RTD1625_LSADC0_DESC_PAD(2),
	.pads[3] = RTD1625_LSADC0_DESC_PAD(3),
	.ext_info = rtd1625_ext_info,
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
	int i, j = 0;

	spin_lock_irqsave(&lsadc->lock, flags);
	for (i = 0; i < indio_dev->num_channels; i++) {
		u32 ints = rtk_lsadc_reg_read(lsadc, desc->pads[i].ints_reg);
		u32 inte = rtk_lsadc_reg_read(lsadc, desc->pads[i].inte_reg);

		if (iio_buffer_enabled(indio_dev) && test_bit(i, indio_dev->active_scan_mask))
			lsadc->scan.buffer[j++] = rtk_lsadc_pad_get_adc_val(lsadc, i);

		if ((ints & inte) == 0)
			continue;

		if (ints & BIT(12))
			lsadc->dmas[i].ppbuf1_ready_ts = ktime_get();
		else if (ints & BIT(11))
			lsadc->dmas[i].ppbuf0_ready_ts = ktime_get();
		rtk_lsadc_reg_write(lsadc, desc->pads[i].ints_reg, 0);
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

	iio_push_to_buffers_with_timestamp(indio_dev, &lsadc->scan, iio_get_time_ns(indio_dev));
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static void rtk_lsadc_pad_set_cmpblks_inte(struct rtk_lsadc_data *lsadc, u32 pad_id, bool en)
{
	const struct rtk_lsadc_pad_desc *pad_desc = &lsadc->desc->pads[pad_id];
	u32 i;
	u32 val = 0, mask = 0;
	unsigned long flags;

	for (i = 0; i < pad_desc->num_cmpblk_regs; i++) {
		mask |= BIT(i + 1);
		if (en && rtk_lsadc_reg_read(lsadc, pad_desc->cmpblk_regs + i * 4) & BIT(15))
			val |= BIT(i + 1);
	}

	if (!mask)
		return;

	spin_lock_irqsave(&lsadc->lock, flags);
	rtk_lsadc_reg_update_bits(lsadc, pad_desc->inte_reg, mask, val);
	spin_unlock_irqrestore(&lsadc->lock, flags);
}

static void rtk_lsadc_rtk_lsadc_set_all_cmpblks_inte(struct iio_dev *indio_dev, bool en)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	int i;

	for (i = 0; i < indio_dev->num_channels; i++)
		rtk_lsadc_pad_set_cmpblks_inte(lsadc, i, en);
}

static int rtk_lsadc_set_trigger_state(struct iio_trigger *trig, bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);

	rtk_lsadc_rtk_lsadc_set_all_cmpblks_inte(indio_dev, state);
	return 0;
}

static const struct iio_trigger_ops rtk_lsadc_trigger_ops = {
	.set_trigger_state = rtk_lsadc_set_trigger_state,
};

static const struct iio_chan_spec rtk_lsadc_channel = {
	.type = IIO_VOLTAGE,
	.indexed = 1,
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_ENABLE),
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
	.scan_type = { .sign = 'u', .realbits = 8, .storagebits = 8, .shift = 0,},
};

static int rtk_lsadc_setup_channels(struct iio_dev *indio_dev, struct rtk_lsadc_data *lsadc)
{
	struct iio_chan_spec *channels;
	int i;

	channels = devm_kcalloc(lsadc->dev, lsadc->desc->num_pads, sizeof(*channels), GFP_KERNEL);
	if (!channels)
		return -ENOMEM;

	indio_dev->channels = channels;
	indio_dev->num_channels = lsadc->desc->num_pads;

	for (i = 0; i < indio_dev->num_channels; i++) {
		channels[i] = rtk_lsadc_channel;
		channels[i].channel = i;
		channels[i].address = i;
		channels[i].scan_index = i;
		channels[i].ext_info = lsadc->desc->ext_info;
	}
	return 0;
}

static int rtk_lsadc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
			      int *val, int *val2, long mask)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	const struct rtk_lsadc_desc *desc = lsadc->desc;
	unsigned long flags;
	int x = chan->channel;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		spin_lock_irqsave(&lsadc->lock, flags);
		*val = rtk_lsadc_pad_get_adc_val(lsadc, x);
		spin_unlock_irqrestore(&lsadc->lock, flags);
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		*val = desc->max_adc_volt_mv;
		*val2 = desc->max_adc_val;
		return IIO_VAL_FRACTIONAL;

	case IIO_CHAN_INFO_ENABLE:
		spin_lock_irqsave(&lsadc->lock, flags);
		*val = rtk_lsadc_pad_get_active(lsadc, x);
		spin_unlock_irqrestore(&lsadc->lock, flags);
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static int rtk_lsadc_write_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
			       int val, int val2, long mask)
{
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	int x = chan->channel;
	unsigned long flags;

	switch (mask) {
	case IIO_CHAN_INFO_ENABLE:
		spin_lock_irqsave(&lsadc->lock, flags);
		rtk_lsadc_pad_set_active(lsadc, x, val);
		spin_unlock_irqrestore(&lsadc->lock, flags);
		return 0;

	default:
		return -EINVAL;
	}
}

static const struct iio_info rtk_lsadc_iio_info = {
	.read_raw  = &rtk_lsadc_read_raw,
	.write_raw = &rtk_lsadc_write_raw,
};

static int rtk_lsadc_runtime_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);
	const struct rtk_lsadc_desc *desc = lsadc->desc;
	int i;

	pm_runtime_get_sync(lsadc->pd_phy);
	pm_runtime_get_sync(lsadc->pd_dma);
	reset_control_deassert(lsadc->rstc);
	clk_prepare_enable(lsadc->clk);

	for (i = 0; i < indio_dev->num_channels; i++)
		rtk_lsadc_reg_write(lsadc, desc->pads[i].pad_reg, 0x80000100);
	rtk_lsadc_reg_update_bits(lsadc, desc->power_reg, desc->power_mask, desc->power_val_on);
	rtk_lsadc_reg_update_bits(lsadc, desc->analog_reg, desc->jd_power_mask,
				  desc->jd_power_val_on);
	msleep(20);

	for (i = 0; i < indio_dev->num_channels; i++)
		rtk_lsadc_reg_write(lsadc, desc->pads[i].ctrl_reg, 0x13f90001);
	return 0;
}

static int rtk_lsadc_runtime_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct rtk_lsadc_data *lsadc = iio_priv(indio_dev);

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

static int rtk_lsadc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct iio_dev *indio_dev;
	struct rtk_lsadc_data *lsadc;
	int ret = -EINVAL;
	int irq;

	indio_dev = devm_iio_device_alloc(dev, sizeof(struct rtk_lsadc_data));
	if (!indio_dev)
		return -ENOMEM;

	lsadc = iio_priv(indio_dev);
	lsadc->dev = dev;
	spin_lock_init(&lsadc->lock);
	init_waitqueue_head(&lsadc->wq);

	lsadc->desc = of_device_get_match_data(dev);
	if (!lsadc->desc) {
		dev_err(dev, "failed to get match data\n");
		return -EINVAL;
	}

	lsadc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(lsadc->base))
		return PTR_ERR(lsadc->base);

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

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return dev_err_probe(dev, irq, "failed to get irq\n");

	indio_dev->dev.parent = dev;
	indio_dev->dev.of_node = dev->of_node;
	indio_dev->name = dev_name(dev);
	indio_dev->info = &rtk_lsadc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->available_scan_masks = lsadc->desc->available_scan_masks;

	ret = rtk_lsadc_setup_channels(indio_dev, lsadc);
	if (ret)
		return ret;

	ret = devm_iio_triggered_buffer_setup(dev, indio_dev, iio_pollfunc_store_time,
					      rtk_lsadc_trigger_handler, NULL);
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

	ret = devm_request_irq(dev, irq, rtk_lsadc_interrupt_handler,
			       IRQF_ONESHOT, dev_name(dev), indio_dev);
	if (ret) {
		dev_err(dev, "unable to request irq#%d: %d\n", irq, ret);
		return ret;
	}

	ret = sysfs_create_group(&indio_dev->dev.kobj, &rtk_lsadc_attr_group);
	if (ret) {
		dev_err(dev, "failed to create attrs\n");
		return ret;
	}

	platform_set_drvdata(pdev, indio_dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	return 0;
}

static int rtk_lsadc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	iio_trigger_put(indio_dev->trig);
	return 0;
}

static void rtk_lsadc_shutdown(struct platform_device *pdev)
{
	pm_runtime_force_suspend(&pdev->dev);
}

static const struct of_device_id rtk_lsadc_of_match[] = {
	{ .compatible = "realtek,rtd1625-lsadc0", .data = &rtd1625_lsadc0_desc, },
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
