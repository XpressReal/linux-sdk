/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _RTK_XDI_INTERNAL_H_
#define _RTK_XDI_INTERNAL_H_

#include <linux/miscdevice.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/dma-buf.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <soc/realtek/uapi/rtk_xdi.h>

struct rtk_xdi_cmd_queue_entry;

typedef void (*rtk_xdi_cmd_complete_cb_t)(struct rtk_xdi_cmd_queue_entry *entry, void *data);

struct rtk_xdi_dev {
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	struct reset_control *rstc;
	struct miscdevice miscdev;

	/* For command queuing */
	struct list_head cmd_queue;
	spinlock_t cmd_queue_lock;
	struct rtk_xdi_cmd_queue_entry *running_cmd;
	struct workqueue_struct *wq;
	struct work_struct cmd_work;
	struct completion hw_done;
};

struct rtk_xdi_context {
	dma_addr_t still_region_dma[3];
	u32 still_idx;
	u32 cmd_id_counter;
};

struct rtk_xdi_hw_cmd {
	dma_addr_t y_addr[4];
	u16 y_pitches[4];
	dma_addr_t c_addr[4];
	u16 c_pitches[4];
	u16 w;
	u16 h;
	struct xdi_flags flags;
	u32 num_params;
	struct xdi_param params[XDI_MAX_PARAMS];
};

struct rtk_xdi_cmd_queue_entry {
	struct rtk_xdi_context *ctx;
	u32 id;
	struct list_head list;
	struct rtk_xdi_hw_cmd hw_cmd;
	int result;
	rtk_xdi_cmd_complete_cb_t cb;
	void *cb_data;
};

extern const struct file_operations rtk_xdi_fops;

static inline u32 rtk_xdi_reg_read(struct rtk_xdi_dev *xdi, u32 reg)
{
	return readl(xdi->base + reg);
}

static inline void rtk_xdi_reg_write(struct rtk_xdi_dev *xdi, u32 reg, u32 val)
{
	writel(val, xdi->base + reg);
}

static inline void rtk_xdi_reg_update_bits(struct rtk_xdi_dev *xdi, u32 reg, u32 mask, u32 val)
{
	u32 tmp;

	tmp = readl(xdi->base + reg);
	tmp &= ~mask;
	tmp |= val & mask;
	writel(tmp, xdi->base + reg);
}

int rtk_xdi_queue_cmd(struct rtk_xdi_dev *xdi, struct rtk_xdi_cmd_queue_entry *entry);
void rtk_xdi_purge_context_cmds(struct rtk_xdi_dev *xdi, struct rtk_xdi_context *ctx);
void rtk_xdi_sync_cmd_context(struct rtk_xdi_dev *xdi, struct rtk_xdi_context *ctx);
void rtk_xdi_start_cmd_work(struct rtk_xdi_dev *xdi);
bool rtk_xdi_param_id_is_valid(u32 id);
bool rtk_xdi_params_is_valid(struct rtk_xdi_dev *xdi, struct xdi_param *params, u32 num_params);

#endif /* _RTK_XDI_INTERNAL_H_ */
