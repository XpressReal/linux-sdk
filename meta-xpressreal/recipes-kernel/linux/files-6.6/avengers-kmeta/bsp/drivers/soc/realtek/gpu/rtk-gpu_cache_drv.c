// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022-2024 Realtek Semiconductor Corp.
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#define pr_fmt(fmt) "gpu_cache: " fmt

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <soc/realtek/uapi/rtk_gpu_cache.h>
#include "rtk-gpu_cache_internal.h"

struct rtk_gpu_cache_frame_data {
	struct gpu_cache_context *context;
	u64 prev_y_adr_begin;
	u64 prev_y_adr_end;
	u64 prev_c_adr_begin;
	u64 prev_c_adr_end;
};

struct rtk_gpu_cache_data {
	struct miscdevice mdev;
	struct device *dev;
	void __iomem *reg;
	struct mutex lock;
	const struct rtk_gpu_cache_desc *desc;

	u32 request_pos;
	struct rtk_gpu_cache_frame_data frames[];
};

static u32 rtk_gpu_cache_reg_read(struct rtk_gpu_cache_data *data, int offset)
{
	u32 val;

	val = readl(data->reg + offset);
	dev_vdbg(data->dev, "%s: %03x: %08x\n", __func__, offset, val);
	return val;
}

static void rtk_gpu_cache_reg_write(struct rtk_gpu_cache_data *data, int offset, u32 val)
{
	dev_vdbg(data->dev, "%s: %03x: %08x\n", __func__, offset, val);
	writel(val, data->reg + offset);
}

static void rtk_gpu_cache_reg_update_bits(struct rtk_gpu_cache_data *data,
					  int offset, u32 mask, u32 val)
{
	u32 v = rtk_gpu_cache_reg_read(data, offset);

	v &= ~mask;
	v |= (val & mask);
	rtk_gpu_cache_reg_write(data, offset, v);
}

struct device *rtk_gpu_cache_dev(struct rtk_gpu_cache_data *data)
{
	return data->dev;
}

struct rtk_gpu_cache_data *rtk_gpu_cache_mdev_to_data(struct miscdevice *mdev)
{
	return container_of(mdev, struct rtk_gpu_cache_data, mdev);
}

void rtk_gpu_cache_lock(struct rtk_gpu_cache_data *data)
{
	mutex_lock(&data->lock);
}

void rtk_gpu_cache_unlock(struct rtk_gpu_cache_data *data)
{
	mutex_unlock(&data->lock);
}

void rtk_gpu_cache_dev_get(struct rtk_gpu_cache_data *data)
{
	pm_runtime_get_sync(data->dev);
}

void rtk_gpu_cache_dev_put(struct rtk_gpu_cache_data *data)
{
	pm_runtime_put_sync(data->dev);
}

static bool frame_info_shared(struct rtk_gpu_cache_data *data)
{
	return data->desc->num_frame_info == 1;
}

static const struct rtk_gpu_cache_param *get_param(struct rtk_gpu_cache_data *data,
							   u32 index)
{
	const struct rtk_gpu_cache_param *param;

	if (index >= data->desc->num_params)
		return NULL;

	param = &data->desc->params[index];
	if (!param->is_const_val && param->reg.offset == 0 &&
		param->reg.msb == 0 && param->reg.lsb == 0)
		return NULL;

	return param;
}

static int rtk_gpu_cache_frame_enabled(struct rtk_gpu_cache_data *data, u32 type,
				       const struct rtk_gpu_cache_reg_frame *f)
{
	return type == 1 ? (rtk_gpu_cache_reg_read(data, f->c.en_offset) & BIT(f->c.en_bit))
			 : (rtk_gpu_cache_reg_read(data, f->y.en_offset) & BIT(f->y.en_bit));
}

static int frm_adr_check(struct rtk_gpu_cache_frame_data *frm, u32 type, u64 adr_begin, u64 adr_end)
{
	return (!type && frm->prev_y_adr_begin == adr_begin && frm->prev_y_adr_end == adr_end) ||
	       (type && frm->prev_c_adr_begin == adr_begin && frm->prev_c_adr_end == adr_end);
}

static void frm_adr_set(struct rtk_gpu_cache_frame_data *frm, u32 type, u64 adr_begin, u64 adr_end)
{
	if (!type) {
		frm->prev_y_adr_begin = adr_begin;
		frm->prev_y_adr_end = adr_end;
	} else {
		frm->prev_c_adr_begin = adr_begin;
		frm->prev_c_adr_end = adr_end;
	}
}

int rtk_gpu_cache_set_frame(struct rtk_gpu_cache_data *data, struct gpu_cache_context *context,
			    struct gpu_cache_set_frame_args *x)
{
	const struct rtk_gpu_cache_reg_frame *f;
	const struct rtk_gpu_cache_reg_frame_offset *fo;
	struct rtk_gpu_cache_frame_data *frm;
	u32 s = data->desc->addr_shift;
	u32 i;

	if (x->index >= data->desc->num_frames) {
		dev_dbg(data->dev, "set_frame: out of index\n");
		return -EINVAL;
	}

	f = &data->desc->reg_frames[x->index];
	frm = &data->frames[x->index];
	fo = x->type ? &f->c : &f->y;

	if (frm->context != context) {
		dev_dbg(data->dev, "set_frame: from different context\n");
		return -EINVAL;
	}

	if (s) {
		u32 m = GENMASK(s - 1, 0);

		if ((x->adr_begin & m) || (x->adr_end & m) || (x->header & m) || (x->payload & m)) {
			dev_dbg(data->dev, "set_frame: address(es) not align to %d\n", 1 << s);
			return -EINVAL;
		}
	}

	if (rtk_gpu_cache_frame_enabled(data, x->type, f)) {
		dev_dbg(data->dev, "set_frame: already in used\n");
		return -EBUSY;
	}

	if (frm_adr_check(frm, x->type, x->adr_begin, x->adr_end)) {
		dev_dbg(data->dev, "set_frame: with the same address range\n");
		return -EINVAL;
	}

	for (i = 0; i < data->desc->num_frames; i++) {
		if (i == x->index)
			continue;

		if (!frm_adr_check(&data->frames[i], x->type, x->adr_begin, x->adr_end))
			continue;

		if (rtk_gpu_cache_frame_enabled(data, x->type, &data->desc->reg_frames[i]))  {
			dev_dbg(data->dev, "set_frame: address already used by frame%u\n", i);
			return -EBUSY;
		}

		frm_adr_set(&data->frames[i], x->type, 0, 0);
	}

	frm_adr_set(frm, x->type, x->adr_begin, x->adr_end);

	rtk_gpu_cache_reg_write(data, fo->adr_begin_offset, x->adr_begin >> s);
	rtk_gpu_cache_reg_write(data, fo->adr_end_offset, x->adr_end >> s);
	rtk_gpu_cache_reg_write(data, fo->header_offset, x->header >> s);
	rtk_gpu_cache_reg_write(data, fo->payload_offset, x->payload >> s);
	rtk_gpu_cache_reg_update_bits(data, fo->en_offset, BIT(fo->en_bit), BIT(fo->en_bit));
	return 0;
}

int rtk_gpu_cache_clear_frame(struct rtk_gpu_cache_data *data, struct gpu_cache_context *context,
			      u32 index, u32 type)
{
	const struct rtk_gpu_cache_reg_frame *f;
	const struct rtk_gpu_cache_reg_frame_offset *fo;
	struct rtk_gpu_cache_frame_data *frm;

	if (index >= data->desc->num_frames) {
		dev_dbg(data->dev, "clear_frame: out of index\n");
		return -EINVAL;
	}

	f = &data->desc->reg_frames[index];
	frm = &data->frames[index];
	fo = type ? &f->c : &f->y;

	if (frm->context != context) {
		dev_dbg(data->dev, "clear_frame: from different context\n");
		return -EINVAL;
	}

	if (!rtk_gpu_cache_frame_enabled(data, type, f)) {
		return 0;
	}

	rtk_gpu_cache_reg_update_bits(data, fo->en_offset, BIT(fo->en_bit), 0);
	rtk_gpu_cache_reg_write(data, fo->adr_begin_offset, 0);
	rtk_gpu_cache_reg_write(data, fo->adr_end_offset, 0);
	rtk_gpu_cache_reg_write(data, fo->header_offset, 0);
	rtk_gpu_cache_reg_write(data, fo->payload_offset, 0);
	return 0;
}

int rtk_gpu_cache_set_frame_info(struct rtk_gpu_cache_data *data, struct gpu_cache_context *context,
				 struct gpu_cache_set_frame_info_args *x)
{
	const struct rtk_gpu_cache_reg_frame_info *fi;
	u32 mask, val;

	if (frame_info_shared(data)) {
		if (x->index || x->pic_height)
			dev_warn(data->dev, "ignore invalid index(%d) or pic_height(%d) in frame_info\n",
				 x->index, x->pic_height);
		x->index = 0;
		x->pic_height = 0;
		fi = &data->desc->reg_frame_info[0];
	} else {
		if (x->index >= data->desc->num_frame_info) {
			dev_dbg(data->dev, "set_frame_info: out of index\n");
			return -EINVAL;
		}

		if (data->frames[x->index].context != context) {
			dev_dbg(data->dev, "set_frame_info: from different context\n");
			return -EINVAL;
		}

		fi = &data->desc->reg_frame_info[x->index];
	}

	mask = GENMASK(fi->decomp_header_pitch_msb, fi->decomp_header_pitch_lsb) |
	       GENMASK(fi->decomp_payload_pitch_msb, fi->decomp_payload_pitch_lsb);
	val = (x->decomp_header_pitch << fi->decomp_header_pitch_lsb) |
	      (x->decomp_payload_pitch << fi->decomp_payload_pitch_lsb);
	rtk_gpu_cache_reg_update_bits(data, fi->decomp_pitch_offset, mask, val);

	rtk_gpu_cache_reg_update_bits(data, fi->gpu_ip_pitch_offset,
				      GENMASK(fi->gpu_ip_pitch_msb, fi->gpu_ip_pitch_lsb),
				      x->gpu_ip_pitch << fi->gpu_ip_pitch_lsb);
	if (fi->pic_height_offset) {
		rtk_gpu_cache_reg_update_bits(data, fi->pic_height_offset,
					      GENMASK(fi->pic_height_msb, fi->pic_height_lsb),
					      x->pic_height << fi->pic_height_lsb);
	}

	return 0;
}

int rtk_gpu_cache_set_param(struct rtk_gpu_cache_data *data, u32 id, u32 value)
{
	const struct rtk_gpu_cache_param *param = get_param(data, id);
	const struct rtk_gpu_cache_reg_field *reg;
	u32 mask, val;

	if (!param)
		return -EINVAL;

	if (param->is_const_val)
		return -EINVAL;

	reg = &param->reg;
	mask = GENMASK(reg->msb, reg->lsb);
	val = value << reg->lsb;

	if (~mask & val) {
		dev_notice(data->dev, "param%d: invalid value(%#x)\n", id, value);
		return -EINVAL;
	}

	rtk_gpu_cache_reg_update_bits(data, reg->offset, mask, val);
	return 0;
}

int rtk_gpu_cache_get_param(struct rtk_gpu_cache_data *data, u32 id, u32 *value)
{
	const struct rtk_gpu_cache_param *param = get_param(data, id);
	const struct rtk_gpu_cache_reg_field *reg;
	u32 mask, val;

	if (!param)
		return -EINVAL;

	if (param->is_const_val) {
		*value = param->const_val;
		return 0;
	}

	reg = &param->reg;
	mask = GENMASK(reg->msb, reg->lsb);
	val = rtk_gpu_cache_reg_read(data, reg->offset);

	*value = (val & mask) >> reg->lsb;
	return 0;
}


int rtk_gpu_cache_request_frame(struct rtk_gpu_cache_data *data,
				struct gpu_cache_context *context,
				struct gpu_cache_request_frame_args *x)
{
	size_t i, j;

	for (i = 0; i < data->desc->num_frames; i++) {
		j = (i + data->request_pos) % data->desc->num_frames;
		if (data->frames[j].context != NULL)
			continue;

		if (x->check_prev_adr) {
			if (frm_adr_check(&data->frames[j], 0, x->y_adr_begin, x->y_adr_end)) {
				dev_info(data->dev, "request_frame: y_frame with the same address range\n");
				continue;
			}

			if (frm_adr_check(&data->frames[j], 1, x->c_adr_begin, x->c_adr_end)) {
				dev_info(data->dev, "request_frame: c_frame with the same address range\n");
				continue;
			}
		}

		data->frames[j].context = context;
		data->request_pos = j + 1;
		return j;
	}
	return -EBUSY;
}

int rtk_gpu_cache_release_frame(struct rtk_gpu_cache_data *data,
				struct gpu_cache_context *context, u32 i)
{
	if (i >= data->desc->num_frames)
		return -EINVAL;

	if (data->frames[i].context != context)
		return -EINVAL;

	rtk_gpu_cache_clear_frame(data, context, i, 0);
	rtk_gpu_cache_clear_frame(data, context, i, 1);
	data->frames[i].context = NULL;
	return 0;
}

void rtk_gpu_cache_release_all_frames_by_context(struct rtk_gpu_cache_data *data,
						 struct gpu_cache_context *context)
{
	size_t i;

	for (i = 0; i < data->desc->num_frames; i++)
		rtk_gpu_cache_release_frame(data, context, i);
}

int rtk_gpu_cache_flush(struct rtk_gpu_cache_data *data)
{
	unsigned int val;
	int ret;
	u32 reg_decomp_status_offset = data->desc->reg_decomp_offset + REG_DECOMP_STATUS;

	ret = readl_poll_timeout(data->reg + reg_decomp_status_offset, val, val & 0x1, 0, 500);
	if (ret == -ETIMEDOUT)
		return -EBUSY;

	rtk_gpu_cache_reg_update_bits(data, reg_decomp_status_offset, 0x2, 0x2);
	for (;;) {

		val = rtk_gpu_cache_reg_read(data, reg_decomp_status_offset);
		if ((val & 0x2) == 0)
			return 0;

		if (signal_pending(current))
			return -ERESTARTSYS;
	}
	return 0;
}

static int rtk_gpu_cache_runtime_suspend(struct device *dev)
{
	return 0;
}

static int rtk_gpu_cache_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops rtk_gpu_cache_dev_pm_ops = {
	.runtime_resume  = rtk_gpu_cache_runtime_resume,
	.runtime_suspend = rtk_gpu_cache_runtime_suspend,
};

static struct platform_driver rtk_gpu_cache_driver;

static int first_match(struct device *dev, const void *data)
{
	return 1;
}

struct rtk_gpu_cache_data *rtk_gpu_cache_data_get(void)
{
	struct device *dev;
	struct rtk_gpu_cache_data *data;

	dev = driver_find_device(&rtk_gpu_cache_driver.driver, NULL, NULL, first_match);
	if (!dev)
		return ERR_PTR(-ENODEV);

	data = dev_get_drvdata(dev);
	if (!data)
		return ERR_PTR(-EINVAL);
	return data;
}

static int rtk_gpu_cache_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct rtk_gpu_cache_desc *desc;
	struct rtk_gpu_cache_data *data;
	int ret;

	desc = of_device_get_match_data(dev);
	if (!desc)
		return -EINVAL;

	data = devm_kzalloc(dev, struct_size(data, frames, desc->num_frames), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->desc = desc;
	data->dev = dev;
	mutex_init(&data->lock);

	data->reg = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(data->reg))
		return PTR_ERR(data->reg);

	data->mdev.minor  = MISC_DYNAMIC_MINOR;
	data->mdev.name   = "gpu_cache";
	data->mdev.fops   = &rtk_gpu_cache_fops;
	data->mdev.parent = NULL;
	ret = misc_register(&data->mdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, data);

	if (desc->support_max_32gb_addr)
		dma_set_mask_and_coherent(dev, DMA_BIT_MASK(35));

	pm_runtime_set_suspended(dev);
	pm_runtime_enable(dev);
	return 0;
}

static int rtk_gpu_cache_remove(struct platform_device *pdev)
{
	struct rtk_gpu_cache_data *data = platform_get_drvdata(pdev);

	misc_deregister(&data->mdev);
	pm_runtime_disable(&pdev->dev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id rtk_gpu_cache_match[] = {
	{ .compatible = "realtek,rtd1319d-gpu-cache", .data = &rtd1319d_desc, },
	{ .compatible = "realtek,rtd1315e-gpu-cache", .data = &rtd1315e_desc, },
	{ .compatible = "realtek,rtd1625-gpu-cache", .data = &rtd1625_desc, },
	{}
};

static struct platform_driver rtk_gpu_cache_driver = {
	.probe    = rtk_gpu_cache_probe,
	.remove   = rtk_gpu_cache_remove,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "rtk-gpu_cache",
		.of_match_table = of_match_ptr(rtk_gpu_cache_match),
		.pm             = &rtk_gpu_cache_dev_pm_ops,
	},
};
module_platform_driver(rtk_gpu_cache_driver);

MODULE_DESCRIPTION("Realtek GPU Cache driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtk-gpu_cache");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
