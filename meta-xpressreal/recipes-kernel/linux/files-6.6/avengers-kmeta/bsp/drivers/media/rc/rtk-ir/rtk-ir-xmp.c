// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * RTK IR Decoder setup for XMP protocol.
 *
 * Copyright (C) 2020 Simon Hsu <simon_hsu@realtek.com>
 */

#include "rtk-ir.h"
#include <linux/bitrev.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

static int rtk_ir_xmp_scancode(struct rtk_ir_scancode_req *request, unsigned int raw)
{
	int i, temp = 0;
	int bits;
	int repeat = 0;

	for(i=0; i<8; i++) {
		bits = (raw & (0xF<<(4*i)))>>(4*i);
		if (i==4 && bits == 0xf)
			return 0;
		if (i==6) {
			if (!(bits & 0x8))
				repeat = 1;
			continue;
		}
		temp = temp + bits;
	}
	temp = (~temp+1) & 0xF;
	if(temp != (raw & 0xF<<24)>>24)
		return 0;

	request->scancode = (raw >> 8) & 0xff;
	request->protocol = RC_PROTO_XMP;
	if (request->scancode != request->last_scancode)
		repeat = 0;
	request->repeat = repeat;

	return 1;
}

static int rtk_ir_xmp_wakeinfo(struct rtk_ir_wakeinfo *info, unsigned int scancode)
{
	info->addr = 0;
	info->addr_msk = 0;
	info->scancode = scancode;
	info->scancode_msk = 0xff00;

	return 0;
}

#ifdef CONFIG_PROC_FS
static int rtk_ir_proc_show(struct seq_file *m, void *v)
{
	struct rtk_ir_hw_decoder *dec = m->private;
	unsigned char owner, tag;

	owner = (dec->last_raw & 0xf0000000) >> 28;
	tag = (dec->last_raw & 0x000f0000) >> 16;

	seq_printf(m, "0x%x", tag | owner << 4);

	return 0;
}

static int rtk_ir_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rtk_ir_proc_show,  pde_data(inode));

	return 0;
}

static struct proc_dir_entry *rtk_ir_pe;
static struct proc_dir_entry *root_pe;

static const struct proc_ops rtk_ir_proc_fops = {
	.proc_open	= rtk_ir_proc_open,
	.proc_read	= seq_read,
	.proc_release	= single_release,
};

static int rtk_ir_proc_create(struct rtk_ir_hw_decoder *dec)
{
	root_pe = proc_mkdir("rtk", NULL);

	rtk_ir_pe = proc_create_data("irda_tao_show", 0444, root_pe, &rtk_ir_proc_fops, dec);
	if (!rtk_ir_pe)
		return -ENOMEM;

	pr_err("xmp %s good\n", __func__);
	return 0;
}

static void rtk_ir_proc_destroy(void)
{
	if (rtk_ir_pe)
		remove_proc_entry("rtk_irda_tao_show", NULL);
}

#else
static int rtk_ir_proc_create(void) { return 0; }
static void rtk_ir_proc_destroy(void) {}
#endif

static void rtk_ir_xmp_init(struct rtk_ir_hw_decoder *dec)
{
	rtk_ir_proc_create(dec);
}

static void rtk_ir_xmp_uninit(struct rtk_ir_hw_decoder *dec)
{
	rtk_ir_proc_destroy();
}


struct rtk_ir_hw_decoder rtk_ir_xmp = {
	.type = RC_PROTO_BIT_XMP,
	.hw_set = COMCAST_EN | IRUE | IRRES | IRIE | 0x1f,
	.scancode = rtk_ir_xmp_scancode,
	.wakeinfo = rtk_ir_xmp_wakeinfo,
	.init = rtk_ir_xmp_init,
	.uninit = rtk_ir_xmp_uninit,
};
