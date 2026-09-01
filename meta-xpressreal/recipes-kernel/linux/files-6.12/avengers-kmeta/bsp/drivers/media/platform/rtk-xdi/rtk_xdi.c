// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitfield.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <soc/realtek/uapi/rtk_xdi.h>
#include "rtk_xdi_internal.h"
#include "rtk_xdi_reg-rtd1635.h"

#define CREATE_TRACE_POINTS
#include "rtk_xdi_trace.h"

#define DEFINE_XDI_PARAM_RAW(reg) \
	[XDI_PARAM_ID_##reg] = XDI_REG_##reg

static const u32 rtd1635_params[] = {
	DEFINE_XDI_PARAM_RAW(CORE_DI_DYNAMIC_0),
	DEFINE_XDI_PARAM_RAW(CORE_DI_DYNAMIC_1),
	DEFINE_XDI_PARAM_RAW(CORE_DI_SMOOTH),
	DEFINE_XDI_PARAM_RAW(CORE_DI_STILL),
	DEFINE_XDI_PARAM_RAW(CORE_DI_VOTE),
	DEFINE_XDI_PARAM_RAW(CORE_DI_TEETH),
	DEFINE_XDI_PARAM_RAW(CORE_DI_WGTFILT_THD),
	DEFINE_XDI_PARAM_RAW(CORE_DI_WGTFILT_WGT),
	DEFINE_XDI_PARAM_RAW(CORE_DI_WGTFILT_WIN0),
	DEFINE_XDI_PARAM_RAW(CORE_DI_WGTFILT_WIN1),
	DEFINE_XDI_PARAM_RAW(CORE_DI_WGTFILT_WIN2),
	DEFINE_XDI_PARAM_RAW(CORE_DI_NOISE),
	DEFINE_XDI_PARAM_RAW(CORE_DI_NOISE_WIN_H),
	DEFINE_XDI_PARAM_RAW(CORE_DI_NOISE_WIN_W),
	DEFINE_XDI_PARAM_RAW(CORE_DI_NOISE_LEVEL_SUM),
	DEFINE_XDI_PARAM_RAW(CORE_DI_NOISE_LEVEL),
	DEFINE_XDI_PARAM_RAW(CORE_DI_C_TYPE),
	DEFINE_XDI_PARAM_RAW(CORE_DI_C_WIN0_H),
	DEFINE_XDI_PARAM_RAW(CORE_DI_C_WIN0_W),
	DEFINE_XDI_PARAM_RAW(CORE_DI_C_WIN1_H),
	DEFINE_XDI_PARAM_RAW(CORE_DI_C_WIN1_W),
	DEFINE_XDI_PARAM_RAW(CORE_DI_C_WIN2_H),
	DEFINE_XDI_PARAM_RAW(CORE_DI_C_WIN2_W),
};

bool rtk_xdi_param_id_is_valid(u32 id)
{
	if (id < 1 || id > ARRAY_SIZE(rtd1635_params))
		return false;
	return true;
}

bool rtk_xdi_params_is_valid(struct rtk_xdi_dev *xdi, struct xdi_param *params, u32 num_params)
{
	u32 i;

	for (i = 0; i < num_params; i++) {
		if (!rtk_xdi_param_id_is_valid(params[i].id))
			return false;
	}
	return true;
}

static void rtk_xdi_cmd_setup_params(struct rtk_xdi_dev *xdi, struct rtk_xdi_cmd_queue_entry *entry)
{
	struct rtk_xdi_hw_cmd *hw_cmd = &entry->hw_cmd;
	u32 i;

	for (i = 0; i < hw_cmd->num_params; i++) {
		if (!rtk_xdi_param_id_is_valid(hw_cmd->params[i].id))
			continue;
		rtk_xdi_reg_write(xdi, rtd1635_params[hw_cmd->params[i].id],
				  hw_cmd->params[i].value);
	}
}

static int rtk_xdi_runtime_suspend(struct device *dev)
{
	struct rtk_xdi_dev *xdi = dev_get_drvdata(dev);

	clk_disable_unprepare(xdi->clk);
	reset_control_assert(xdi->rstc);

	return 0;
}

static int rtk_xdi_runtime_resume(struct device *dev)
{
	struct rtk_xdi_dev *xdi = dev_get_drvdata(dev);

	reset_control_deassert(xdi->rstc);
	clk_prepare_enable(xdi->clk);

	return 0;
}

static const struct dev_pm_ops rtk_xdi_pm_ops = {
	SET_RUNTIME_PM_OPS(rtk_xdi_runtime_suspend, rtk_xdi_runtime_resume, NULL)
};

static irqreturn_t rtk_xdi_isr(int irq, void *dev_id)
{
	struct rtk_xdi_dev *xdi = dev_id;
	u32 int_status;

	int_status = rtk_xdi_reg_read(xdi, XDI_REG_INTST);
	if (int_status & 0x2) {
		rtk_xdi_reg_write(xdi, XDI_REG_INTST, 0x2);
		complete(&xdi->hw_done);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

int rtk_xdi_queue_cmd(struct rtk_xdi_dev *xdi, struct rtk_xdi_cmd_queue_entry *entry)
{
	unsigned long flags;

	spin_lock_irqsave(&xdi->cmd_queue_lock, flags);
	list_add_tail(&entry->list, &xdi->cmd_queue);
	spin_unlock_irqrestore(&xdi->cmd_queue_lock, flags);
	return 0;
}

void rtk_xdi_start_cmd_work(struct rtk_xdi_dev *xdi)
{
	schedule_work(&xdi->cmd_work);
}

void rtk_xdi_purge_context_cmds(struct rtk_xdi_dev *xdi, struct rtk_xdi_context *ctx)
{
	LIST_HEAD(head);
	unsigned long flags;
	struct rtk_xdi_cmd_queue_entry *pos, *n;

	spin_lock_irqsave(&xdi->cmd_queue_lock, flags);
	list_for_each_entry_safe(pos, n, &xdi->cmd_queue, list) {
		if (pos->ctx == ctx)
			list_move_tail(&pos->list, &head);
	}
	spin_unlock_irqrestore(&xdi->cmd_queue_lock, flags);

	list_for_each_entry_safe(pos, n, &head, list) {
		pos->result = -ECANCELED;
		if (pos->cb)
			pos->cb(pos, pos->cb_data);
	}
}

void rtk_xdi_sync_cmd_context(struct rtk_xdi_dev *xdi, struct rtk_xdi_context *ctx)
{
	bool should_wait = false;
	struct rtk_xdi_cmd_queue_entry *entry = READ_ONCE(xdi->running_cmd);

	if (entry && entry->ctx == ctx)
		should_wait = true;

	if (!should_wait)
		return;

	wait_for_completion_timeout(&xdi->hw_done, msecs_to_jiffies(100));
}

static void rtk_xdi_cmd_setup(struct rtk_xdi_dev *xdi, struct rtk_xdi_cmd_queue_entry *entry)
{
	struct rtk_xdi_hw_cmd *hw_cmd = &entry->hw_cmd;
	struct rtk_xdi_context *ctx = entry->ctx;
	u32 p_y_addr = 0, p_c_addr = 0, p_y_pitch = 0, p_c_pitch = 0;
	u32 n_y_addr = 0, n_c_addr = 0, n_y_pitch = 0, n_c_pitch = 0;

	writeb(0x0, xdi->base + XDI_REG_CORE);

	/* 0 -> output (Write-Back) */
	rtk_xdi_reg_write(xdi, XDI_REG_WB_SEQ_SA_Y, hw_cmd->y_addr[0] >> 4);
	rtk_xdi_reg_write(xdi, XDI_REG_WB_SEQ_SA_C, hw_cmd->c_addr[0] >> 4);
	rtk_xdi_reg_write(xdi, XDI_REG_WB_SEQ_PITCH,
			  FIELD_PREP(XDI_REG_FIELD_C_PTICH, hw_cmd->c_pitches[0]) |
			  FIELD_PREP(XDI_REG_FIELD_Y_PTICH, hw_cmd->y_pitches[0]));

	/* 1 -> current */
	rtk_xdi_reg_write(xdi, XDI_REG_CORE_SEQ_SA_C_Y, hw_cmd->y_addr[1] >> 4);
	rtk_xdi_reg_write(xdi, XDI_REG_CORE_SEQ_SA_C_C, hw_cmd->c_addr[1] >> 4);
	rtk_xdi_reg_write(xdi, XDI_REG_CORE_SEQ_PITCH_C,
			  FIELD_PREP(XDI_REG_FIELD_C_PTICH, hw_cmd->c_pitches[1]) |
			  FIELD_PREP(XDI_REG_FIELD_Y_PTICH, hw_cmd->y_pitches[1]));

	if  (hw_cmd->flags.mode != 3) {
		p_y_addr = hw_cmd->y_addr[2] >> 4;
		p_c_addr = hw_cmd->c_addr[2] >> 4;
		p_y_pitch = hw_cmd->y_pitches[2];
		p_c_pitch = hw_cmd->c_pitches[2];

		n_y_addr = hw_cmd->y_addr[3] >> 4;
		n_c_addr = hw_cmd->c_addr[3] >> 4;
		n_y_pitch = hw_cmd->y_pitches[3];
		n_c_pitch = hw_cmd->c_pitches[3];
	}

	rtk_xdi_reg_write(xdi, XDI_REG_CORE_SEQ_SA_P_Y, p_y_addr);
	rtk_xdi_reg_write(xdi, XDI_REG_CORE_SEQ_SA_P_C, p_c_addr);
	rtk_xdi_reg_write(xdi, XDI_REG_CORE_SEQ_PITCH_P,
			  FIELD_PREP(XDI_REG_FIELD_C_PTICH, p_c_pitch) |
			  FIELD_PREP(XDI_REG_FIELD_Y_PTICH, p_y_pitch));

	rtk_xdi_reg_write(xdi, XDI_REG_CORE_SEQ_SA_N_Y, n_y_addr);
	rtk_xdi_reg_write(xdi, XDI_REG_CORE_SEQ_SA_N_C, n_c_addr);
	rtk_xdi_reg_write(xdi, XDI_REG_CORE_SEQ_PITCH_N,
			  FIELD_PREP(XDI_REG_FIELD_C_PTICH, n_c_pitch) |
			  FIELD_PREP(XDI_REG_FIELD_Y_PTICH, n_y_pitch));

	rtk_xdi_reg_write(xdi, XDI_REG_CORE,
			  FIELD_PREP(XDI_REG_FIELD_ST, hw_cmd->flags.st) |
			  FIELD_PREP(XDI_REG_FIELD_NV21, hw_cmd->flags.nv21) |
			  FIELD_PREP(XDI_REG_FIELD_PPC10B, hw_cmd->flags.ppc10b) |
			  FIELD_PREP(XDI_REG_FIELD_F422, hw_cmd->flags.f422) |
			  FIELD_PREP(XDI_REG_FIELD_TOPFIELD, hw_cmd->flags.topfield));

	rtk_xdi_reg_write(xdi, XDI_REG_CORE_SIZE,
			  FIELD_PREP(XDI_REG_FIELD_W, hw_cmd->w) |
			  FIELD_PREP(XDI_REG_FIELD_H, hw_cmd->h));

	rtk_xdi_reg_write(xdi, XDI_REG_CORE_DI,
			  FIELD_PREP(XDI_REG_FIELD_WEAVE_TEETH_EN,
				     hw_cmd->flags.weave_teeth_en) |
			  FIELD_PREP(XDI_REG_FIELD_NOISE_LEVEL_EN,
				     hw_cmd->flags.noise_level_en) |
			  FIELD_PREP(XDI_REG_FIELD_CU, hw_cmd->flags.cu) |
			  FIELD_PREP(XDI_REG_FIELD_LIGHT_COMB_EN,
				     hw_cmd->flags.light_comb_en) |
			  FIELD_PREP(XDI_REG_FIELD_WEIGHT_FILTER_EN,
				     hw_cmd->flags.weight_filter_en) |
			  FIELD_PREP(XDI_REG_FIELD_MF_SELECT, hw_cmd->flags.mf_select) |
			  FIELD_PREP(XDI_REG_FIELD_COMB_CHK_EN, hw_cmd->flags.comb_chk_en) |
			  FIELD_PREP(XDI_REG_FIELD_CHK_USE_MAX_MF,
				     hw_cmd->flags.chk_use_max_mf) |
			  FIELD_PREP(XDI_REG_FIELD_WB_CHK_RESULT,
				     hw_cmd->flags.wb_chk_result) |
			  FIELD_PREP(XDI_REG_FIELD_WB_MAX_DIFF, hw_cmd->flags.wb_max_diff) |
			  FIELD_PREP(XDI_REG_FIELD_USE_WB_DIFF, hw_cmd->flags.use_wb_diff) |
			  FIELD_PREP(XDI_REG_FIELD_SOURCE, hw_cmd->flags.source) |
			  FIELD_PREP(XDI_REG_FIELD_BOB_CHK0_EN, hw_cmd->flags.bob_chk0_en) |
			  FIELD_PREP(XDI_REG_FIELD_BOB_CHK1_EN, hw_cmd->flags.bob_chk1_en) |
			  FIELD_PREP(XDI_REG_FIELD_BOB_CHK2_EN, hw_cmd->flags.bob_chk2_en) |
			  FIELD_PREP(XDI_REG_FIELD_CHK4_EN, hw_cmd->flags.chk4_en) |
			  FIELD_PREP(XDI_REG_FIELD_SMOOTH_EN, hw_cmd->flags.smooth_en) |
			  FIELD_PREP(XDI_REG_FIELD_STILL_EN, hw_cmd->flags.still_en) |
			  FIELD_PREP(XDI_REG_FIELD_STILL_RESET, hw_cmd->flags.still_reset) |
			  FIELD_PREP(XDI_REG_FIELD_VOTE_EN, hw_cmd->flags.vote_en) |
			  FIELD_PREP(XDI_REG_FIELD_DBG_BLEND_RATIO,
				     hw_cmd->flags.dbg_blend_ratio) |
			  FIELD_PREP(XDI_REG_FIELD_DBG_COMBING, hw_cmd->flags.dbg_combing) |
			  FIELD_PREP(XDI_REG_FIELD_DBG_BOB, hw_cmd->flags.dbg_bob) |
			  FIELD_PREP(XDI_REG_FIELD_HCS_420_SEL_PN,
				     hw_cmd->flags.hcs_420_sel_pn) |
			  FIELD_PREP(XDI_REG_FIELD_MODE, hw_cmd->flags.mode));

	rtk_xdi_reg_write(xdi, XDI_REG_WB,
			  FIELD_PREP(XDI_REG_FIELD_WB_F420, hw_cmd->flags.wb_f420) |
			  FIELD_PREP(XDI_REG_FIELD_WB_TPC_NUM, hw_cmd->flags.wb_tpc_num) |
			  FIELD_PREP(XDI_REG_FIELD_WB_PPC10B, hw_cmd->flags.wb_ppc10b) |
			  FIELD_PREP(XDI_REG_FIELD_WB_P010, hw_cmd->flags.wb_p010) |
			  FIELD_PREP(XDI_REG_FIELD_WB_TR, hw_cmd->flags.wb_tr));

	if (hw_cmd->flags.use_wb_diff) {
		rtk_xdi_reg_write(xdi, XDI_REG_CORE_MA_FR2,
				  ctx->still_region_dma[ctx->still_idx] >> 4);
		rtk_xdi_reg_write(xdi, XDI_REG_CORE_MA_FR,
				  ctx->still_region_dma[ctx->still_idx + 1] >> 4);
		rtk_xdi_reg_write(xdi, XDI_REG_CORE_MA_FW,
				  ctx->still_region_dma[ctx->still_idx + 2] >> 4);
		ctx->still_idx = (ctx->still_idx + 3) % 3;
	} else {
		rtk_xdi_reg_write(xdi, XDI_REG_CORE_MA_FR2, 0);
		rtk_xdi_reg_write(xdi, XDI_REG_CORE_MA_FR, 0);
		rtk_xdi_reg_write(xdi, XDI_REG_CORE_MA_FW, 0);
	}

	rtk_xdi_cmd_setup_params(xdi, entry);
}

static void rtk_xdi_work_handler(struct work_struct *work)
{
	struct rtk_xdi_dev *xdi = container_of(work, struct rtk_xdi_dev, cmd_work);
	struct rtk_xdi_cmd_queue_entry *entry;
	unsigned long flags;
	int ret;

	while (1) {
		spin_lock_irqsave(&xdi->cmd_queue_lock, flags);
		entry = list_first_entry_or_null(&xdi->cmd_queue,
						 struct rtk_xdi_cmd_queue_entry, list);
		if (entry) {
			list_del_init(&entry->list);
			WRITE_ONCE(xdi->running_cmd, entry);
		}
		spin_unlock_irqrestore(&xdi->cmd_queue_lock, flags);

		if (!entry)
			break;

		reinit_completion(&xdi->hw_done);
		rtk_xdi_cmd_setup(xdi, entry);
		rtk_xdi_reg_write(xdi, XDI_REG_INTEN, 0x9);
		rtk_xdi_reg_write(xdi, XDI_REG_FC, 0x3);

		trace_xdi_cmd_start(entry->ctx, entry->id);

		ret = wait_for_completion_timeout(&xdi->hw_done, msecs_to_jiffies(50));

		rtk_xdi_reg_write(xdi, XDI_REG_INTEN, 0x8);
		rtk_xdi_reg_write(xdi, XDI_REG_FC, 0x2);

		WRITE_ONCE(xdi->running_cmd, NULL);

		entry->result = !ret ? -ETIMEDOUT : 0;
		trace_xdi_cmd_end(entry->ctx, entry->id, entry->result);
		if (entry->cb)
			entry->cb(entry, entry->cb_data);
	}
}

static int rtk_xdi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_xdi_dev *xdi;
	int ret;
	int irq;

	xdi = devm_kzalloc(dev, sizeof(*xdi), GFP_KERNEL);
	if (!xdi)
		return -ENOMEM;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(35));
	if (ret) {
		dev_err(dev, "failed to set dma mask\n");
		return ret;
	}

	xdi->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(xdi->base))
		return PTR_ERR(xdi->base);

	xdi->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(xdi->clk)) {
		dev_err(dev, "failed to get clock\n");
		return PTR_ERR(xdi->clk);
	}

	xdi->rstc = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(xdi->rstc)) {
		dev_err(dev, "failed to get reset control\n");
		return PTR_ERR(xdi->rstc);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(dev, irq, rtk_xdi_isr, IRQF_SHARED, dev_name(dev), xdi);
	if (ret) {
		dev_err(dev, "failed to request irq\n");
		return ret;
	}

	xdi->dev = dev;
	platform_set_drvdata(pdev, xdi);

	init_completion(&xdi->hw_done);
	INIT_LIST_HEAD(&xdi->cmd_queue);
	spin_lock_init(&xdi->cmd_queue_lock);
	xdi->running_cmd = NULL;

	xdi->wq = create_singlethread_workqueue("rtk-xdi");
	if (!xdi->wq) {
		dev_err(dev, "failed to create workqueue\n");
		return -ENOMEM;
	}
	INIT_WORK(&xdi->cmd_work, rtk_xdi_work_handler);

	xdi->miscdev.minor = MISC_DYNAMIC_MINOR;
	xdi->miscdev.name = "xdi";
	xdi->miscdev.fops = &rtk_xdi_fops;
	xdi->miscdev.parent = dev;

	ret = misc_register(&xdi->miscdev);
	if (ret) {
		dev_err(dev, "failed to register misc device\n");
		destroy_workqueue(xdi->wq);
		return ret;
	}

	pm_runtime_set_autosuspend_delay(dev, 200);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(dev);

	return 0;
}

static void rtk_xdi_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_xdi_dev *xdi = platform_get_drvdata(pdev);

	pm_runtime_disable(dev);
	misc_deregister(&xdi->miscdev);
	WARN_ON(!list_empty(&xdi->cmd_queue));
	flush_workqueue(xdi->wq);
	destroy_workqueue(xdi->wq);
}

static const struct of_device_id rtk_xdi_of_match[] = {
	{ .compatible = "realtek,rtd1635-xdi", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rtk_xdi_of_match);

static struct platform_driver rtk_xdi_driver = {
	.probe		= rtk_xdi_probe,
	.remove		= rtk_xdi_remove,
	.driver		= {
		.name	= "rtk-xdi",
		.of_match_table = rtk_xdi_of_match,
		.pm	= &rtk_xdi_pm_ops,
	},
};
module_platform_driver(rtk_xdi_driver);

MODULE_AUTHOR("Edgar Lee <cylee12@realtek.com>");
MODULE_DESCRIPTION("Realtek XDI Platform Driver");
MODULE_LICENSE("GPL v2");
