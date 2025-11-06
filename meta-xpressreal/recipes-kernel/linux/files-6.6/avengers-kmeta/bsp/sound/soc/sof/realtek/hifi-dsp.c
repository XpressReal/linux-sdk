// SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause)
//
// Copyright 2019 Realtek
//
// Author: YH Hsueh <yh_hsieh@realtek.com>
// Author: Simon Hsu <simon_hsu@realtek.com>
//

#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include <sound/sof.h>
#include <soc/realtek/rtk-krpc-agent.h>
#include <soc/realtek/rtk_ipc_shm.h>
#include "../ops.h"
#include "../sof-audio.h"
#include "../sof-priv.h"
#include "../sof-of-dev.h"
#include "../sof-pci-dev.h"

static char *fw_path;
module_param(fw_path, charp, 0444);
MODULE_PARM_DESC(fw_path, "alternate path for SOF firmware.");

static char *tplg_path;
module_param(tplg_path, charp, 0444);
MODULE_PARM_DESC(tplg_path, "alternate path for SOF topology.");

#define MBOX_OFFSET 0x5fb000

#define HIFI_SOF_CREATE 0x20000040
#define HIFI_SOF_CREATE_DONE 0x20000041
#define HIFI_SOF_IPC_CMD 0x20000042
#define HIFI_SOF_REPLY_OF_SEND_MSG 0x20000043

#define HIFI_SOF2K_HOST 0x20001001

#define ISO_POWER_CTRL 0x98007FD0
#define ISO_HIFI0_SRAM_PWR4 0x98007248
#define ISO_HIFI0_SRAM_PWR5 0x9800724C

#define SYS_PLL_HIFI1 0x980001D8
#define SYS_PLL_HIFI2 0x980001DC
#define SYS_PLL_SSC_DIG_HIFI0 0x980006E0
#define SYS_PLL_SSC_DIG_HIFI1 0x980006E4
#define HIFI_PLL_796MHZ 0x0001C000

#define RTK_MAX_STREAM 8
#define BOOT_MAX_RETRY 1000

#define PCPU_IPC_CMD_ADDR_OFFSET 0x470
#define PCPU_IPC_ARG_ADDR_OFFSET 0x474
#define PCPU_IPC_INTR_ADDR_OFFSET 0xa80
#define PCPU_POWER_ON_HIFI 0x825b5008
#define PCPU_POWER_DOWN_HIFI 0x825b5000

struct sof_rtk_adsp_stream {
	struct snd_sof_dev *sdev;
	size_t posn_offset;
//	struct sof_rtk_stream stream;
	int stream_tag;
	int active;
};

struct rtk_hifi_priv {
	struct snd_sof_dev *sdev;
	struct rtk_krpc_ept_info *sof_ept;
	struct sof_rtk_adsp_stream stream_buf[RTK_MAX_STREAM];
	struct regmap *intr_regmap;
	struct regmap *ipc_regmap;
};

static int hifi_poweron(struct snd_sof_dev *sdev)
{
	int sleeptime_us = 1000;
	int val;
	int ret = 0;
	struct rtk_hifi_priv *priv = sdev->pdata->hw_pdata;

	regmap_write(priv->ipc_regmap, PCPU_IPC_ARG_ADDR_OFFSET, 0);

	regmap_write(priv->ipc_regmap, PCPU_IPC_CMD_ADDR_OFFSET, PCPU_POWER_ON_HIFI);

	regmap_read(priv->intr_regmap, PCPU_IPC_INTR_ADDR_OFFSET, &val);

	if(!(val & BIT(3)))
		regmap_write(priv->intr_regmap, PCPU_IPC_INTR_ADDR_OFFSET, val|BIT(3)|BIT(0));

	ret = regmap_read_poll_timeout(priv->ipc_regmap, PCPU_IPC_ARG_ADDR_OFFSET, val,
			val==0x1, sleeptime_us, sleeptime_us*1000);

	if(ret) {
		pr_err("[HIFI] ERROR: failed to power on HiFi\n");
		ret = -EINVAL;
		goto poweron_end;
	}

	 regmap_write(priv->ipc_regmap, PCPU_IPC_ARG_ADDR_OFFSET, 0);

poweron_end:
        return ret;
}

static int hifi_powerdown(struct snd_sof_dev *sdev)
{
        int sleeptime_us = 1000;
        int val;
        int ret = 0;
        struct rtk_hifi_priv *priv = sdev->pdata->hw_pdata;

	regmap_write(priv->ipc_regmap, PCPU_IPC_ARG_ADDR_OFFSET, 0);

        regmap_write(priv->ipc_regmap, PCPU_IPC_CMD_ADDR_OFFSET, PCPU_POWER_DOWN_HIFI);

        regmap_read(priv->intr_regmap, PCPU_IPC_INTR_ADDR_OFFSET, &val);


        if(!(val & BIT(3)))
                regmap_write(priv->intr_regmap, PCPU_IPC_INTR_ADDR_OFFSET, val|BIT(3)|BIT(0));

        ret = regmap_read_poll_timeout(priv->ipc_regmap, PCPU_IPC_ARG_ADDR_OFFSET, val,
                        val==0x1, sleeptime_us, sleeptime_us*1000);

        if(ret) {
                pr_err("[HIFI] ERROR: failed to power down HiFi\n");
                ret = -EINVAL;
                goto powerdown_end;
        }

         regmap_write(priv->ipc_regmap, PCPU_IPC_ARG_ADDR_OFFSET, 0);

powerdown_end:
        return ret;
}

static char *prepare_rpc_data(struct rtk_krpc_ept_info *krpc_ept_info,
			      u32 command, u32 param1, u32 param2, int *len,
			      int receive_reply)
{
	struct rpc_struct *rpc;
	u32 *tmp;
	char *buf;

	*len = sizeof(struct rpc_struct) + 3 * sizeof(u32);
	buf = kmalloc(sizeof(struct rpc_struct) + 3 * sizeof(u32), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	// maybe procedureID could be used for sof identification
	rpc = (struct rpc_struct *)buf;
	rpc->programID = KERNELID;
	rpc->versionID = KERNELID;
	rpc->procedureID = 0;

	if (receive_reply)
		rpc->taskID = krpc_ept_info->id;
	else
		rpc->taskID = 0;

	rpc->sysTID = krpc_ept_info->id;
	rpc->sysPID = krpc_ept_info->id;

	rpc->parameterSize = 3 * sizeof(u32);
	rpc->mycontext = 0;
	tmp = (u32 *)(buf + sizeof(struct rpc_struct));
	*tmp = command;
	*(tmp + 1) = param1;
	*(tmp + 2) = param2;

	return buf;
}

static int rtk_krpc_hifi_send(struct rtk_krpc_ept_info *sof_ept, int cmd,
			      int reply)
{
	unsigned int rpc_ret = 0;
	int ret = -1, len;
	char *buf;

	buf = prepare_rpc_data(sof_ept, cmd, sof_ept->id, 0x0, &len, reply);
	if (IS_ERR(buf)) {
		pr_err("[%s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	sof_ept->retval = &rpc_ret;
	ret = rtk_send_rpc(sof_ept, buf, len);
	if (ret < 0) {
		pr_err("[%s] send rpc failed\n", sof_ept->name);
		goto rpc_finish;
	}

	if (!reply) {
		rpc_ret = S_OK;
	} else {
		if (!wait_for_completion_timeout(&sof_ept->ack, RPC_TIMEOUT)) {
			pr_err("%s: kernel rpc timeout...\n", sof_ept->name);
			rtk_krpc_dump_ringbuf_info(sof_ept);
		}
	}

	if (rpc_ret != S_OK) {
		pr_err("%s rpc return not S_OK\n", __func__);
		ret = -1;
		goto rpc_finish;
	}

	//pr_info("%s %s 0x%x success\n", __FILE__, __func__, cmd);
	ret = 0;

rpc_finish:
	kfree(buf);
exit:
	return ret;
}
static void rtk_get_reply(struct snd_sof_dev *sdev)
{
	struct snd_sof_ipc_msg *msg = sdev->msg;
	struct sof_ipc_reply reply;

	int ret = 0;

	if (!msg) {
		dev_warn(sdev->dev, "unexpected ipc interrupt\n");
		return;
	}
	/* get reply */
	sof_mailbox_read(sdev, sdev->host_box.offset, &reply, sizeof(reply));
	if (reply.error < 0) {
		memcpy(msg->reply_data, &reply, sizeof(reply));
		ret = reply.error;
	} else {
		/* reply has correct size? */
		if (reply.hdr.size != msg->reply_size) {
			dev_err(sdev->dev,
				"error: reply expected %zu got %u bytes\n",
				msg->reply_size, reply.hdr.size);
			ret = -EINVAL;
		}

		/* read the message */
		if (msg->reply_size > 0)
			sof_mailbox_read(sdev, sdev->host_box.offset,
					 msg->reply_data, msg->reply_size);
	}

	msg->reply_error = ret;
}

static int rtk_krpc_hifi_cb(struct rtk_krpc_ept_info *sof_ept, char *buf)
{
	struct rtk_hifi_priv *priv = sof_ept->priv;
	struct rpc_struct *rpc = (struct rpc_struct *)buf;
	struct rpc_struct *rrpc;
	char replybuf[sizeof(struct rpc_struct) + 2 * sizeof(uint32_t)];
	unsigned int *tmp;
	int size, ret;
	unsigned long flags;

	if (rpc->programID == REPLYID) {
		tmp = (u32 *)(buf + sizeof(struct rpc_struct));
		*(sof_ept->retval) = *(tmp + 1);

		complete(&sof_ept->ack);
		return 0;
	}

	size = rpc->parameterSize;
	tmp = (uint32_t *)(buf + sizeof(struct rpc_struct));

	spin_lock_irqsave(&priv->sdev->ipc_lock, flags);
	if (*(tmp + 0) == HIFI_SOF2K_HOST) {
		rtk_get_reply(priv->sdev);
		snd_sof_ipc_reply(priv->sdev, 0);
	} else {
		snd_sof_ipc_msgs_rx(priv->sdev);
		rtk_krpc_hifi_send(priv->sof_ept, HIFI_SOF_REPLY_OF_SEND_MSG,
				   0);
	}
	spin_unlock_irqrestore(&priv->sdev->ipc_lock, flags);

	if (rpc->taskID) {
		pr_info("%s reply kernel rpc %d", __func__, rpc->taskID);
		rrpc = (struct rpc_struct *)replybuf;
		/* fill the RPC_STRUCT... */
		rrpc->programID = REPLYID;
		rrpc->versionID = REPLYID;
		rrpc->procedureID = 0;
		rrpc->taskID = 0;
		rrpc->sysTID = 0;
		rrpc->sysPID = 0;
		rrpc->parameterSize = 2 * sizeof(uint32_t);
		rrpc->mycontext = rpc->mycontext;

		/* fill the parameters... */
		tmp = (uint32_t *)(replybuf + sizeof(struct rpc_struct));
		*(tmp + 0) = rpc->taskID; /* FIXME: should be 64bit */
		*(tmp + 1) = 0;
		ret = rtk_send_rpc(sof_ept, replybuf, sizeof(replybuf));
		if (ret != sizeof(replybuf)) {
			pr_err("ERROR in send kernel RPC...\n");
			return -1;
		}
	}

	return 0;
}

static int rtk_get_mailbox_offset(struct snd_sof_dev *sdev)
{
	return MBOX_OFFSET;
}

static int rtk_get_window_offset(struct snd_sof_dev *sdev, u32 id)
{
	return MBOX_OFFSET;
}

static int rtk_hifi_probe(struct snd_sof_dev *sdev)
{
	struct platform_device *pdev =
		container_of(sdev->dev, struct platform_device, dev);
	struct device_node *np = pdev->dev.of_node;
	struct rtk_krpc_ept_info *sof_ept;
	struct rtk_hifi_priv *priv;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (WARN_ON(!np))
		return -ENODEV;

	sof_ept = of_krpc_ept_info_get(np, 0);
	if (IS_ERR(sof_ept))
		return dev_err_probe(&pdev->dev, PTR_ERR(sof_ept),
				     "failed to get HIFI krpc ept : 0x%lx\n",
				     PTR_ERR(sof_ept));

	ret = krpc_info_init(sof_ept, "rtk_sof", rtk_krpc_hifi_cb);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "failed to init HIFI krpc ept info");

        priv->intr_regmap = syscon_regmap_lookup_by_phandle(sdev->dev->of_node, "intr-syscon");
        if(IS_ERR_OR_NULL(priv->intr_regmap)) {
                pr_err("get intr regmap FAIL!");
                return -EINVAL;
        }
        priv->ipc_regmap = syscon_regmap_lookup_by_phandle(sdev->dev->of_node, "ipc-syscon");
        if(IS_ERR_OR_NULL(priv->ipc_regmap)) {
                pr_err("get ipc regmap FAIL!");
                return -EINVAL;
        }

	priv->sof_ept = sof_ept;
	priv->sof_ept->priv = priv;
	priv->sdev = sdev;
	sdev->pdata->hw_pdata = priv;

	sdev->bar[SOF_FW_BLK_TYPE_SRAM] =
		devm_ioremap(sdev->dev, 0x0f800000, 0x800000);
	sdev->mailbox_bar = SOF_FW_BLK_TYPE_SRAM;

	sdev->dsp_box.offset = rtk_get_mailbox_offset(sdev);

	return 0;
}

static int rtk_hifi_remove(struct snd_sof_dev *sdev)
{
	struct rtk_hifi_priv *priv = sdev->pdata->hw_pdata;

	krpc_info_deinit(priv->sof_ept);
	krpc_ept_info_put(priv->sof_ept);

	hifi_powerdown(sdev);

	return 0;
}

static int rtk_hifi_shutdown(struct snd_sof_dev *sdev)
{
	return snd_sof_suspend(sdev->dev);
}

void set_hifi_pll_and_ssc_control(uint32_t freq_setting)
{
	void __iomem *map_bit;

	map_bit = ioremap(SYS_PLL_HIFI2, 0x120);
	if (readl(map_bit) == 0x3)
		return;

	/* Set AUCPU PLL & SSC control:
	 * Set 0x9800_01DC 0x5, PLL OEB=1, RSTB=0, POW=1
	 * Set 0x9800_01DC 0x7, PLL OEB=1, RSTB=1, POW=1
	 * Set 0x9800_06E0 0xC, CKSSC_INV=1, SSC_DIG_RSTB=1, OC_EN=0, SSC_EN=0
	 * Set 0x9800_01D8 (Electrical specification, no need setting in simulation)
	 * Set 0x9800_06E4 0x1C800, 810Mz, SSC_NCODE_T=39, SSC_FCODE=0
	 * Set 0x9800_06E0 0xD, CKSSC_INV=1, SSC_DIG_RSTB=1, OC_EN=1, SSC_EN=0
	 * Set 0x9800_01DC 0x3, PLL OEB=0, RSTB=1, POW=1
	 * Need wait 200us for PLL oscillating
	 */
	writel(0x00000005, map_bit);
	writel(0x00000007, map_bit);

	map_bit = ioremap(SYS_PLL_SSC_DIG_HIFI0, 0x120);
	writel(0x0000000c, map_bit);
	/* Set RS value of PLL to increase performance, please refer to Note 2 of
	 * https://wiki.realtek.com/pages/viewpage.action?pageId=136516076
	 */
	map_bit = ioremap(SYS_PLL_HIFI1, 0x120);
	writel(0x02060000, map_bit);

	/* Set frequency to 796.5Mhz */
	map_bit = ioremap(SYS_PLL_SSC_DIG_HIFI1, 0x120);
	writel(freq_setting, map_bit);

	map_bit = ioremap(SYS_PLL_SSC_DIG_HIFI0, 0x120);
	writel(0x0000000d, map_bit);
	udelay(200);

	map_bit = ioremap(SYS_PLL_HIFI2, 0x120);
	writel(0x00000003, map_bit);

	iounmap(map_bit);
}

static void rtk_hifi_log_shm_setup(struct snd_sof_dev *sdev)
{
	struct device_node *log_dev;
	const __be32 *prop;
	int len;
	uint32_t log_addr, log_size, log_level;
	struct avcpu_syslog_struct __iomem *avlog_p;
	struct rtk_ipc_shm __iomem *ipc = (void __iomem *)IPC_SHM_VIRT;
	void __iomem *map_bit;

	/* Locate the log buffer */
	log_dev = of_find_node_by_path("/reserved-memory/hlog0");
	if (log_dev) {
		prop = of_get_property(log_dev, "reg", &len);
		if (prop) {
			if (len != (2 * sizeof(__be32)))
				dev_info(sdev->dev,
					 "Invalid hlog0 property setting.\n");
			else {
				log_addr = cpu_to_be32(*prop);
				log_size = cpu_to_be32(*(++prop));
				dev_info(
					sdev->dev,
					"Found hlog0 buffer at 0x%x, size:0x%x.\n",
					log_addr, log_size);
			}
		}
	}
	of_node_put(log_dev);

	/* Get the log level setting */
	log_dev = of_find_node_by_path("/rtk_avcpu/hlog");
	if (log_dev) {
		prop = of_get_property(log_dev, "lvl", &len);
		if (prop) {
			if (len != sizeof(__be32))
				dev_info(sdev->dev,
					 "Invalid hlog0 level setting.\n");
			else {
				log_level = cpu_to_be32(*prop);
				dev_info(sdev->dev,
					 "Found hlog0 level setting:0x%x.\n",
					 log_level);
			}
		}
	}
	of_node_put(log_dev);

	/* Update log setting to RPC common area */
	avlog_p = (struct avcpu_syslog_struct *)(IPC_SHM_VIRT +
						 offsetof(struct rtk_ipc_shm,
							  hifi_printk_buffer));
	if (avlog_p) {
		/* Check if buffer info is valid */
		if ((log_addr && (!log_size)) || ((!log_addr) && log_size))
			dev_err(sdev->dev,
				"Invalid hlog0 setting (addr:0x%x, size:0x%x)",
				log_addr, log_size);
		else {
			avlog_p->log_buf_addr = log_addr;
			avlog_p->log_buf_len = log_size;
			avlog_p->con_start = log_level;
		}
	}
	map_bit = ioremap(0x9801aa80, 0x4);
	writel(0x11, map_bit);
	iounmap(map_bit);

	/* Update FW RPC flag for deferred probe */
	writel(0x55665566, &ipc->hifi_rpc_flag);
}

static int rtk_hifi_pre_fw_run(struct snd_sof_dev *sdev)
{
	int ret;
	void __iomem *map_bit;

	rtk_hifi_log_shm_setup(sdev);

	/* Clear DSP box head before boot */
	ret = 0;
	sof_mailbox_write(sdev, sdev->dsp_box.offset, (void *)&ret, 4);

	map_bit=ioremap(0x9801ad00,0x120);
	writel(0x0f800000,map_bit);
	iounmap(map_bit);
	hifi_poweron(sdev);


	return 0;
}

static int rtk_hifi_run(struct snd_sof_dev *sdev)
{
	struct rtk_hifi_priv *priv = sdev->pdata->hw_pdata;
	int32_t head, i = 0;

	/* Snoop DSP box to check if FW RPC is ready */
	do {
		sof_mailbox_read(sdev, sdev->dsp_box.offset, &head, 4);
		usleep_range(1000, 10000);
	} while ((head == 0) && (++i < BOOT_MAX_RETRY));
	if (i == BOOT_MAX_RETRY) {
		dev_err(sdev->dev,
			"[%s] DSP boot failed after %d snooping, DSP box head=%08x\n",
			__func__, i, head);
		return -ENODEV;
	}

	rtk_krpc_hifi_send(priv->sof_ept, HIFI_SOF_CREATE, 0);

	return 0;
}

static int rtk_hifi_reset(struct snd_sof_dev *sdev)
{
	hifi_powerdown(sdev);

	return 0;
}

static int rtk_host_send_msg(struct snd_sof_dev *sdev,
			     struct snd_sof_ipc_msg *msg)
{
	struct rtk_hifi_priv *priv = sdev->pdata->hw_pdata;

	sof_mailbox_write(sdev, sdev->host_box.offset, msg->msg_data,
			  msg->msg_size);

	rtk_krpc_hifi_send(priv->sof_ept, HIFI_SOF_IPC_CMD, 0);

	return 0;
}

static int rtk_get_bar_index(struct snd_sof_dev *sdev, u32 type)
{
	return type;
}

struct snd_soc_acpi_mach *rtk_machine_select(struct snd_sof_dev *sdev)
{
	struct snd_sof_pdata *sof_pdata = sdev->pdata;
	const struct sof_dev_desc *desc = sof_pdata->desc;
	struct snd_soc_acpi_mach *mach;

	for (mach = desc->machines; mach->board; mach++) {
		if (of_machine_is_compatible(mach->board)) {
			sof_pdata->tplg_filename = mach->sof_tplg_filename;
			sof_pdata->machine = mach;

			dev_info(sdev->dev, "%s, tplg: %s\n", __func__,
				 mach->sof_tplg_filename);

			mach->pdata = sdev->dev->of_node;
			if (!mach->pdata)
				dev_warn(sdev->dev, "get of_node failed\n");

			return mach;
		}
	}

	return NULL;
}

static struct sof_rtk_adsp_stream *rtk_hifi_stream_get(struct snd_sof_dev *sdev,
						       int tag)
{
	struct rtk_hifi_priv *priv = sdev->pdata->hw_pdata;
	struct sof_rtk_adsp_stream *stream = priv->stream_buf;
	int i;

	for (i = 0; i < RTK_MAX_STREAM; i++, stream++) {
		if (stream->active)
			continue;

		/* return stream if tag not specified*/
		if (!tag) {
			stream->active = 1;
			return stream;
		}

		/* check if this is the requested stream tag */
		if (stream->stream_tag == tag) {
			stream->active = 1;
			return stream;
		}
	}

	return NULL;
}

static int rtk_hifi_stream_put(struct snd_sof_dev *sdev,
			       struct sof_rtk_adsp_stream *rtk_stream)
{
	struct rtk_hifi_priv *priv = sdev->pdata->hw_pdata;
	struct sof_rtk_adsp_stream *stream = priv->stream_buf;
	int i;

	/* Free an active stream */
	for (i = 0; i < RTK_MAX_STREAM; i++, stream++) {
		if (stream == rtk_stream) {
			stream->active = 0;
			return 0;
		}
	}

	return -EINVAL;
}

static int rtk_hifi_pcm_hw_params(struct snd_sof_dev *sdev,
				  struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_sof_platform_stream_params *platform_params)
{
	return 0;
}

static int rtk_hifi_pcm_open(struct snd_sof_dev *sdev,
			     struct snd_pcm_substream *substream)
{
	struct sof_rtk_adsp_stream *stream;

	stream = rtk_hifi_stream_get(sdev, 0);
	if (!stream)
		return -ENODEV;

	pr_err("%s success\n", __func__);
	substream->runtime->private_data = stream;

	return 0;
}

static int rtk_hifi_pcm_close(struct snd_sof_dev *sdev,
			      struct snd_pcm_substream *substream)
{
	struct sof_rtk_adsp_stream *stream;

	stream = substream->runtime->private_data;
	if (!stream) {
		dev_err(sdev->dev, "No open stream\n");
		return -EINVAL;
	}

	substream->runtime->private_data = NULL;

	return rtk_hifi_stream_put(sdev, stream);
}

snd_pcm_uframes_t rtk_hifi_pcm_pointer(struct snd_sof_dev *sdev,
				       struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *scomp = sdev->component;
	struct snd_sof_pcm *spcm;
	struct sof_ipc_stream_posn posn;
	struct snd_sof_pcm_stream *stream;
	snd_pcm_uframes_t pos;

	spcm = snd_sof_find_spcm_dai(scomp, rtd);
	if (!spcm) {

		dev_warn_ratelimited(sdev->dev,
				     "warn: can't find PCM with DAI ID %d\n",
				     rtd->dai_link->id);
		return 0;
	}

	stream = &spcm->stream[substream->stream];

	// Todo: The content of input in snd_sof_ipc_msg_data can define by us.
	snd_sof_ipc_msg_data(sdev, stream, &posn, sizeof(posn));

	memcpy(&stream->posn, &posn, sizeof(posn));
	pos = spcm->stream[substream->stream].posn.host_posn;
	pos = bytes_to_frames(substream->runtime, pos);


	return pos;
}

static struct snd_soc_dai_driver rtk_hifi_dai[] = {
{
	.name = "SOF_DL2",
	.playback = {
		.channels_min = 1,
		.channels_max = 8,
	},
//	.capture = {
//		.channels_min = 1,
//		.channels_max = 8,
//	},
},
{
	.name = "SOF_UL4",
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
	},
},
{
	.name = "SOF_DMIC",
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
	},
},
{
        .name = "SOF_DP",
        .playback = {
                .channels_min = 1,
                .channels_max = 8,
        },
},
{
        .name = "SOF_HDMI",
        .playback = {
                .channels_min = 1,
                .channels_max = 8,
        },
},

};

static int rtk_hifi_resume(struct snd_sof_dev *sdev)
{
	hifi_poweron(sdev);
	return 0;
}
static int rtk_hifi_suspend(struct snd_sof_dev *sdev, u32 target_state)
{
	hifi_powerdown(sdev);
        return 0;
}

struct snd_sof_dsp_ops sof_hifi_ops = {
	/* probe and remove */
	.probe = rtk_hifi_probe,
	.remove = rtk_hifi_remove,
	.shutdown = rtk_hifi_shutdown,

	/* DSP core boot */
	.pre_fw_run = rtk_hifi_pre_fw_run,
	.run = rtk_hifi_run,
	.reset = rtk_hifi_reset,

	/* Block IO */
	.block_read = sof_block_read,
	.block_write = sof_block_write,

	/* Register IO */
	.write = sof_io_write,
	.read = sof_io_read,
	.write64 = sof_io_write64,
	.read64 = sof_io_read64,

	/* ipc */
	.send_msg = rtk_host_send_msg,
	.ipc_msg_data = sof_ipc_msg_data,
	/* Mailbox IO */
	.mailbox_read	= sof_mailbox_read,
	.mailbox_write	= sof_mailbox_write,
	.set_stream_data_offset = sof_set_stream_data_offset,

	.get_mailbox_offset = rtk_get_mailbox_offset,
	.get_window_offset = rtk_get_window_offset,

	/* misc */
	.get_bar_index = rtk_get_bar_index,

	/* machine driver */
	.machine_select = rtk_machine_select,

	/* stream callbacks */
	.pcm_open = rtk_hifi_pcm_open,
	.pcm_hw_params = rtk_hifi_pcm_hw_params,
	.pcm_close = rtk_hifi_pcm_close,
	.pcm_pointer = rtk_hifi_pcm_pointer,

	/* firmware loading */
	.load_firmware = snd_sof_load_firmware_memcpy,

	/* DAI drivers */
	.drv = rtk_hifi_dai,
	.num_drv = ARRAY_SIZE(rtk_hifi_dai),

	.resume = rtk_hifi_resume,
	.suspend = rtk_hifi_suspend,

	/* ALSA HW info flags */
	.hw_info = SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
		   SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_PAUSE |
		   SNDRV_PCM_INFO_NO_PERIOD_WAKEUP
};

static struct snd_soc_acpi_mach sof_rtk_machs[] = {
	{
		.board = "realtek,rtd1920s",
		.drv_name = "rtk-hifi",
		.sof_tplg_filename = "sof-adsp-rtd1920s.tplg",
	},
	{},
};

static struct sof_dev_desc snd_sof_rtd1920s_desc = {
	.machines = sof_rtk_machs,
	.ipc_supported_mask	= BIT(SOF_IPC),
	.ipc_default		= SOF_IPC,
	.default_fw_path = {
		[SOF_IPC] = "rtd1920s",
	},
	.default_tplg_path = {
		[SOF_IPC] = "rtd1920s",
	},
	.default_fw_filename = {
		[SOF_IPC] = "sof-adsp-rtd1920s.ri",
	},
	.nocodec_tplg_filename = "",
	.ops = &sof_hifi_ops,
};

static const struct of_device_id sof_of_rtk_ids[] = {
	{ .compatible = "realtek,rtd1920s-hifi",
	  .data = &snd_sof_rtd1920s_desc },
	{},
};
MODULE_DEVICE_TABLE(of, sof_of_rtk_ids);
static void sof_rtk_probe_complete(struct device *dev)
{
	pr_info("%s %d\n", __func__, __LINE__);

	/* allow runtime_pm */
	pm_runtime_set_autosuspend_delay(dev, SND_SOF_SUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
}

static int sof_rtk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct snd_sof_pdata *sof_pdata;
	const struct sof_dev_desc *desc;

	sof_pdata = devm_kzalloc(dev, sizeof(*sof_pdata), GFP_KERNEL);
	if (!sof_pdata)
		return -ENOMEM;

	desc = device_get_match_data(dev);
	if (!desc)
		return -ENODEV;

	if (!desc->ops) {
		dev_err(dev, "error: no matching DT descriptor ops\n");
		return -ENODEV;
	}

	sof_pdata->desc = desc;
	sof_pdata->dev = &pdev->dev;
	sof_pdata->fw_filename = desc->default_fw_filename;



	if (fw_path)
		sof_pdata->fw_filename_prefix = fw_path;
	else
		sof_pdata->fw_filename_prefix =
			sof_pdata->desc->default_fw_path;

	if (tplg_path)
		sof_pdata->tplg_filename_prefix = tplg_path;
	else
		sof_pdata->tplg_filename_prefix =
			sof_pdata->desc->default_tplg_path;

	/* set callback to be called on successful device probe to enable runtime_pm */
	sof_pdata->sof_probe_complete = sof_rtk_probe_complete;

	/* call sof helper for DSP hardware probe */
	return snd_sof_device_probe(dev, sof_pdata);
}

static int sof_rtk_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	/* call sof helper for DSP hardware remove */
	snd_sof_device_remove(&pdev->dev);

	return 0;
}

/* DT driver definition */
static struct platform_driver snd_sof_rtk_driver = {
	.probe = sof_of_probe,
	.remove = sof_of_remove,
	.driver = {
		.name = "sof-audio-of-rtk",
		.of_match_table = sof_of_rtk_ids,
		.pm = &sof_of_pm,
	},
};
module_platform_driver(snd_sof_rtk_driver);

MODULE_LICENSE("Dual BSD/GPL");
