#include <log.h>
#include <dm.h>
#include <clk-uclass.h>
#include <asm/io.h>
#include <linux/iopoll.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>
#include "rtk_i2c.h"

/* I2C Registers */
#define I2C_CON			0x00
#define I2C_TAR			0x04
#define I2C_SAR			0x08
#define I2C_DATA_CMD		0x10
#define I2C_SS_SCL_HCNT         0x14
#define I2C_SS_SCL_LCNT         0x18
#define I2C_FS_SCL_HCNT         0x1c
#define I2C_FS_SCL_LCNT         0x20
#define I2C_HS_SCL_HCNT         0x24
#define I2C_HS_SCL_LCNT         0x28
#define I2C_INTR_STAT		0x2C
#define I2C_INTR_MASK		0x30
#define I2C_RAW_INTR_STAT	0x34
#define I2C_RX_TL		0x38
#define I2C_TX_TL		0x3C
#define I2C_CLR_INT		0x40
#define I2C_CLR_RX_UNDER	0x44
#define I2C_CLR_RX_OVER		0x48
#define I2C_CLR_TX_OVER		0x4c
#define I2C_CLR_RD_REQ		0x50
#define I2C_CLR_TX_ABRT		0x54
#define I2C_CLR_RX_DONE		0x58
#define I2C_CLR_ACTIVITY	0x5c
#define I2C_CLR_STOP_DET	0x60
#define I2C_CLR_START_DET	0x64
#define I2C_CLR_GEN_CALL	0x68
#define I2C_ENABLE		0x6C
#define I2C_IC_STATUS		0x70
#define I2C_TXFLR		0x74
#define I2C_RXFLR		0x78
#define I2C_SDA_HOLD		0x7c
#define I2C_TX_ABRT_SOURCE	0x80
#define I2C_ENABLE_STATUS	0x9c
#define I2C_COMP_PARAM_1	0xF4

/* I2C_CONTROL Masks */
#define MASTER_EN		(1UL << 0)
#define TEN_BIT_SLAVE		(1UL << 3)
#define TEN_BIT_MASTER		(1UL << 4)
#define RESTART_EN		(1UL << 5)
#define SLAVE_DISABLE		(1UL << 6)
#define TX_EMPTY_CTL		(1UL << 8)
#define SPEED_MSK		0x06
#define SPEED_SS		0x02
#define SPEED_FS		0x04
#define SPEED_HS		0x06

/* I2C_TAR Masks */
#define TAR_TEN_BITADDR		(1UL << 12)

#define STOP_CMD		(1UL << 9)
#define RESTART_CMD		(1UL << 10)

/* INTR_MASK */
#define RX_UNDER		(1UL << 0)
#define RX_OVER			(1UL << 1)
#define RX_FULL			(1UL << 2)
#define TX_OVER			(1UL << 3)
#define TX_EMPTY		(1UL << 4)
#define RD_REQ			(1UL << 5)
#define TX_ABRT			(1UL << 6)
#define RX_DONE			(1UL << 7)
#define ACTIVITY		(1UL << 8)
#define STOP_DET		(1UL << 9)
#define START_DET		(1UL << 10)
#define GEN_CALL		(1UL << 11)
#define INT_DEFAULT_MASK       (RX_FULL | TX_EMPTY | TX_ABRT | STOP_DET)

/* I2C_IC_STATUS */
#define IC_ACTIVITY		(1UL << 0)
#define MST_ACTIVITY		(1UL << 5)
#define SLV_ACTIVITY		(1UL << 6)

#define STATUS_IDLE		0x0
#define STATUS_W_IN_PROGRESS	0x1
#define STATUS_R_IN_PROGRESS	0x2
#define STATUS_W_TAR_CHANGE	0x4

/*
 * hardware abort codes from the TX_ABRT_SOURCE register
 *
 * only expected abort codes are listed here
 * refer to the datasheet for the full list
 */
#define ABRT_7B_ADDR_NOACK	0
#define ABRT_10ADDR1_NOACK	1
#define ABRT_10ADDR2_NOACK	2
#define ABRT_TXDATA_NOACK	3
#define ABRT_GCALL_NOACK	4
#define ABRT_GCALL_READ		5
#define ABRT_SBYTE_ACKDET	7
#define ABRT_SBYTE_NORSTRT	9
#define ABRT_10B_RD_NORSTRT	10
#define ABRT_MASTER_DIS		11
#define ABRT_LOST		12

#define TX_ABRT_7B_ADDR_NOACK	(1UL << ABRT_7B_ADDR_NOACK)
#define TX_ABRT_10ADDR1_NOACK	(1UL << ABRT_10ADDR1_NOACK)
#define TX_ABRT_10ADDR2_NOACK	(1UL << ABRT_10ADDR2_NOACK)
#define TX_ABRT_TXDATA_NOACK	(1UL << ABRT_TXDATA_NOACK)
#define TX_ABRT_GCALL_NOACK	(1UL << ABRT_GCALL_NOACK)
#define TX_ABRT_GCALL_READ	(1UL << ABRT_GCALL_READ)
#define TX_ABRT_SBYTE_ACKDET	(1UL << ABRT_SBYTE_ACKDET)
#define TX_ABRT_SBYTE_NORSTRT	(1UL << ABRT_SBYTE_NORSTRT)
#define TX_ABRT_10B_RD_NORSTRT	(1UL << ABRT_10B_RD_NORSTRT)
#define TX_ABRT_MASTER_DIS	(1UL << ABRT_MASTER_DIS)
#define TX_ABRT_LOST		(1UL << ABRT_LOST)

#define TX_ABRT_NOACK		(TX_ABRT_7B_ADDR_NOACK | \
				 TX_ABRT_10ADDR1_NOACK | \
				 TX_ABRT_10ADDR2_NOACK | \
				 TX_ABRT_TXDATA_NOACK | \
				 TX_ABRT_GCALL_NOACK)

#define RTK_PROCESS_I2C_0	0x8400ff3d
#define RTK_PROCESS_I2C_0_READ	0x8400ff43

#define SDA_DEL_EN
#define I2C_CLK_SRC		27000 /* unit Khz */

#define IGLOO_CA_TYPE		0x4F

#ifdef SDA_DEL_EN
static int SDA_DEL_SHIFT[] = { 0x84, 0x80, 0x80, 0x84, 0x88, 0x8C, 0x90, 0xF0 };
#define I2C_SDA_DEL_MASK	(0x1FF)
#define I2C_SDA_DEL_EN		(0x00000001<<8)
#define I2C_SDA_DEL_SEL(x)	((x & 0x1F)) /* Delay time: (unit 518ns)*/
#define SDA_DEL_518NS		1
#define I2C0_SDA_DEL_520NS    4
#define SDA_DEL_1036NS		2
#define SDA_DEL_1554NS		3
#define SDA_DEL_2072NS		4
#define SDA_DEL_2590NS		5
#endif

static char *abort_sources[] = {
	[ABRT_7B_ADDR_NOACK] =
		"slave address not acknowledged (7bit mode)",
	[ABRT_10ADDR1_NOACK] =
		"first address byte not acknowledged (10bit mode)",
	[ABRT_10ADDR2_NOACK] =
		"second address byte not acknowledged (10bit mode)",
	[ABRT_TXDATA_NOACK] =
		"data not acknowledged",
	[ABRT_GCALL_NOACK] =
		"no acknowledgement for a general call",
	[ABRT_GCALL_READ] =
		"read after general call",
	[ABRT_SBYTE_ACKDET] =
		"start byte acknowledged",
	[ABRT_SBYTE_NORSTRT] =
		"trying to send start byte when restart is disabled",
	[ABRT_10B_RD_NORSTRT] =
		"trying to read when restart is disabled (10bit mode)",
	[ABRT_MASTER_DIS] =
		"trying to use disabled adapter",
	[ABRT_LOST] =
		"lost arbitration",
};

static void rtk_i2c_xfer_msg(struct rtk_i2c_priv *priv);
static void rtk_i2c_xfer_read(struct rtk_i2c_priv *priv);

static void rtk_i2c_int_disable(struct rtk_i2c_priv *priv, unsigned int mask)
{
	unsigned int int_en;

	int_en = readl(priv->base + I2C_INTR_MASK);
	writel(int_en & ~mask, priv->base + I2C_INTR_MASK);
}

static void rtk_i2c_int_enable(struct rtk_i2c_priv *priv, unsigned int mask)
{
	unsigned int int_en;

	int_en = readl(priv->base + I2C_INTR_MASK);
	writel(int_en | mask, priv->base + I2C_INTR_MASK);
}

static inline int rtk_i2c_high_speed_supported(struct rtk_i2c_priv *priv)
{
	return priv->quirks && priv->quirks->high_speed;
}

static int rtk_i2c_set_speed(struct rtk_i2c_priv *priv)
{
	unsigned int KHz = priv->bus_freq_hz / 1000;
	unsigned int scl_time;
	unsigned int div_h = 0;
	unsigned int div_l = 0;
	unsigned int fs_hcnt = 0;
	unsigned int fs_lcnt = 0;
	unsigned int clk_time;
	unsigned int sda_hold_ns = 0;
	unsigned int val;
	int max_speed = rtk_i2c_high_speed_supported(priv) ? 3400 : 800;

	if (KHz < 10 || KHz > max_speed) {
		log_err("speed %d out of range\n", KHz);
		return -1;
	}

	clk_time = 37;			/*use 27MHZ crystal, one clock 37ns*/
	scl_time = (1000000 / KHz) / 2;	/* the time ns need for SCL */
	if (scl_time % clk_time) {
		if ((scl_time % clk_time) > clk_time / 2)
			scl_time += (clk_time - (scl_time % clk_time));
		else
			scl_time -= (scl_time % clk_time);
	}

	if (rtk_i2c_high_speed_supported(priv)) {
		if (KHz == 100) {
			div_h = 524;
			div_l = 532;
		} else if (KHz == 400) {
			div_h = 106;
			div_l = 146;
		} else if (KHz == 3400) {
			fs_hcnt = 106;
			fs_lcnt = 146;
			div_h = 6;
			div_l = 17;
		}
	} else {
		if (KHz < 400) {
			div_h = (scl_time / clk_time) - 8;
			div_l = (scl_time / clk_time);
		} else {
			div_h = 21;
			div_l = 35;
		}
	}

	// if (priv->timings.scl_rise_ns)
	// 	div_h = (priv->timings.scl_rise_ns * I2C_CLK_SRC) / MICRO;
	// if (priv->timings.scl_fall_ns)
	// 	div_l = (priv->timings.scl_fall_ns * I2C_CLK_SRC) / MICRO;

	writel(0, priv->base + I2C_ENABLE);

	val = readl(priv->base + I2C_CON);
	if (KHz <= 100) {
		writel((val & ~SPEED_MSK) | SPEED_SS, priv->base + I2C_CON);
		writel(div_h, priv->base + I2C_SS_SCL_HCNT);
		writel(div_l, priv->base + I2C_SS_SCL_LCNT);
	} else if (KHz == 400) {
		writel((val & ~SPEED_MSK) | SPEED_FS, priv->base + I2C_CON);
		writel(div_h, priv->base + I2C_FS_SCL_HCNT);
		writel(div_l, priv->base + I2C_FS_SCL_LCNT);
	} else if (KHz == 3400) {
		writel((val & ~SPEED_MSK) | SPEED_HS, priv->base + I2C_CON);
		writel(fs_hcnt, priv->base + I2C_FS_SCL_HCNT);
		writel(fs_lcnt, priv->base + I2C_FS_SCL_LCNT);
		writel(div_h, priv->base + I2C_HS_SCL_HCNT);
		writel(div_l, priv->base + I2C_HS_SCL_LCNT);
		sda_hold_ns = 0x5;
	}

	if (priv->nr != 0 && sda_hold_ns == 0 && ((1000000 / KHz) / 4) > 600 )
		sda_hold_ns = (((1000000 / KHz) / 4 - 600) * 27) / 1000;

	writel(sda_hold_ns, priv->base + I2C_SDA_HOLD);

	log_debug("i2c sda hold time = 0x%x\n", sda_hold_ns);

#ifdef SDA_DEL_EN
	val = readl(priv->irqbase + SDA_DEL_SHIFT[priv->nr]);
	val &= ~I2C_SDA_DEL_MASK;

	if (rtk_i2c_high_speed_supported(priv)) {
		if (KHz == 3400)
			val = 0x0;
		else
			val |= I2C_SDA_DEL_EN | I2C_SDA_DEL_SEL(I2C0_SDA_DEL_520NS);

	} else {
		val |= I2C_SDA_DEL_EN | I2C_SDA_DEL_SEL(SDA_DEL_518NS);
	}
	writel(val, priv->irqbase + SDA_DEL_SHIFT[priv->nr]);
#endif
	return 0;
}

static int rtk_i2c_init(struct rtk_i2c_priv *priv)
{
	if (priv->nr == 7)
		writel(BIT(6) | BIT(7), priv->irqbase + 0x90);

	writel(0, priv->base + I2C_ENABLE);
	writel(0, priv->base + I2C_INTR_MASK);

	priv->tx_fifo_depth =
		(((readl(priv->base + I2C_COMP_PARAM_1) >> 16) & 0xFF) + 1);
	priv->rx_fifo_depth =
		(((readl(priv->base + I2C_COMP_PARAM_1) >> 8) & 0xFF) + 1);

	writel(0, priv->base + I2C_TX_TL);
	writel(0, priv->base + I2C_RX_TL);

	return rtk_i2c_set_speed(priv);
}

static rtk_i2c_recover_bus(struct rtk_i2c_priv *priv)
{
	int ret;

	rtk_i2c_reset_assert(priv);
	clk_disable(&priv->clk);
	clk_enable(&priv->clk);
	rtk_i2c_reset_deassert(priv);

}

static void rtk_i2c_handle_tx_abort(struct rtk_i2c_priv *priv)
{
	unsigned long abort_source = priv->abort_source;
	int i;

	if (abort_source & TX_ABRT_NOACK) {
		for_each_set_bit(i, &abort_source, ARRAY_SIZE(abort_sources))
			log_err("%s: %s\n", __func__, abort_sources[i]);
			printf("%s: %s\n", __func__, abort_sources[i]);
		return;
	}

	for_each_set_bit(i, &abort_source, ARRAY_SIZE(abort_sources))
		log_err("%s: %s\n", __func__, abort_sources[i]);
}

static int rtk_i2c_wait_bus_not_busy(struct rtk_i2c_priv *priv)
{
	unsigned int val;
	int ret;

	ret = read_poll_timeout(readl, val, !(val & IC_ACTIVITY), 1100, 20000,
				(priv->base + I2C_IC_STATUS));
	if (ret) {
		log_warning("timeout waiting for bus ready\n");

		rtk_i2c_recover_bus(priv);

		val = readl(priv->base + I2C_IC_STATUS);
		if (!(val & IC_ACTIVITY))
			ret = 0;
	}
	return ret;
}

static void __rtk_i2c_disable(struct rtk_i2c_priv *priv)
{
	int timeout = 100;
	unsigned int status;

	do {
		writel(0, priv->base + I2C_ENABLE);

		status = readl(priv->base + I2C_ENABLE_STATUS);
		if ((status & 1) == 0)
			return;
		/*
		 * Wait 10 times the signaling period of the highest I2C
		 * transfer supported by the driver (for 400KHz this is
		 * 25us) as described in the DesignWare I2C databook.
		 */
		udelay(25);
	} while (timeout--);

	log_warning("timeout in disabling adapter\n");
}

static void rtk_i2c_disable(struct rtk_i2c_priv *priv)
{
	__rtk_i2c_disable(priv);

	writel(0, priv->base + I2C_INTR_MASK);
	readl(priv->base + I2C_CLR_INT);
}

static unsigned int rtk_i2c_clear_intrbits(struct rtk_i2c_priv *priv)
{
	unsigned int status;

	status = readl(priv->base + I2C_INTR_STAT);

	if (status & RX_UNDER)
		readl(priv->base + I2C_CLR_RX_UNDER);
	if (status & RX_OVER)
		readl(priv->base + I2C_CLR_RX_OVER);
	if (status & TX_OVER)
		readl(priv->base + I2C_CLR_TX_OVER);
	if (status & RD_REQ)
		readl(priv->base + I2C_CLR_RD_REQ);
	if (status & TX_ABRT) {
		priv->abort_source = readl(priv->base + I2C_TX_ABRT_SOURCE);
		readl(priv->base + I2C_CLR_TX_ABRT);
	}
	if (status & RX_DONE)
		readl(priv->base + I2C_CLR_RX_DONE);
	if (status & ACTIVITY)
		readl(priv->base + I2C_CLR_ACTIVITY);
	if (status & STOP_DET)
		readl(priv->base + I2C_CLR_STOP_DET);
	if (status & START_DET)
		readl(priv->base + I2C_CLR_START_DET);
	if (status & GEN_CALL)
		readl(priv->base + I2C_CLR_GEN_CALL);

	return status;
}

static int rtk_i2c_check_intr_status(struct rtk_i2c_priv *priv)
{
	unsigned int status, enabled;

	for(int i=0; i<1000; i++) {
		status = readl(priv->base + I2C_INTR_STAT);
		if (status & TX_ABRT || status & STOP_DET)
			break;

		udelay(50);
	}


	enabled = readl(priv->base + I2C_ENABLE);
	status = readl(priv->base + I2C_INTR_STAT);

	if (!enabled || !(status & ~ACTIVITY)) {
		return 0;
	}

	rtk_i2c_clear_intrbits(priv);

	if (status & TX_ABRT) {
		priv->msg_err = -EPROTO;
		priv->status = STATUS_IDLE;
		rtk_i2c_int_disable(priv, ~0);
		goto out;
	}
	if (status & RX_FULL) {
		rtk_i2c_xfer_read(priv);
	}

	if (status & TX_EMPTY) {
		rtk_i2c_xfer_msg(priv);
	}

out:
	return 0;
}

static void rtk_i2c_xfer_msg(struct rtk_i2c_priv *priv)
{
	struct i2c_msg *msgs = priv->msgs;
	unsigned int addr = msgs[priv->msg_w_idx].addr;
	unsigned int buf_len = priv->tx_buf_len;
	unsigned char *buf = priv->tx_buf;
	int tx_limit, rx_limit;
	bool restart = false;
	unsigned int intr_mask = 0;

	for (; priv->msg_w_idx < priv->msgs_num; priv->msg_w_idx++) {
		if (msgs[priv->msg_w_idx].addr != addr) {
			priv->status |= STATUS_W_TAR_CHANGE;
			break;
		}
		if (msgs[priv->msg_w_idx].len == 0) {
			log_err("%s: invalid message length\n", __func__);
			priv->msg_err = -EINVAL;
			break;
		}
		if (priv->status & STATUS_W_TAR_CHANGE) {
			writel(0, priv->base + I2C_ENABLE);
			if (msgs[priv->msg_w_idx].flags & I2C_M_TEN)
				writel((msgs[priv->msg_w_idx].addr & 0x3FF) |
					TAR_TEN_BITADDR, priv->base + I2C_TAR);
			else
				writel(msgs[priv->msg_w_idx].addr & 0x7F,
					priv->base + I2C_TAR);
			writel(1, priv->base + I2C_ENABLE);
			restart = true;
			priv->status &= ~STATUS_W_TAR_CHANGE;
		}
		if (!(priv->status & STATUS_W_IN_PROGRESS)) {
			buf = msgs[priv->msg_w_idx].buf;
			buf_len = msgs[priv->msg_w_idx].len;
			if (priv->msg_w_idx > 0)
				restart = true;
		}
		tx_limit = priv->tx_fifo_depth - readl(priv->base + I2C_TXFLR);
		rx_limit = priv->rx_fifo_depth - readl(priv->base + I2C_RXFLR);

		while (buf_len > 0 && tx_limit > 0 && rx_limit > 0) {
			unsigned int cmd = 0;

			if ((priv->msg_w_idx == priv->msgs_num - 1) &&
			   buf_len == 1)
				cmd |= STOP_CMD;
			if (restart) {
				cmd |= RESTART_CMD;
				restart = false;
			}

			if (msgs[priv->msg_w_idx].flags & I2C_M_RD) {
				if (priv->rx_outstanding >= priv->rx_fifo_depth)
					break;
				writel(cmd | 0x100, priv->base + I2C_DATA_CMD);
				rx_limit--;
				priv->rx_outstanding++;
			} else
				writel(cmd | *buf++, priv->base + I2C_DATA_CMD);
			tx_limit--; buf_len--;
		}
		priv->tx_buf = buf;
		priv->tx_buf_len = buf_len;

		if (buf_len > 0) {
			/* more bytes to be written */
			priv->status |= STATUS_W_IN_PROGRESS;
			break;
		}
		priv->status &= ~STATUS_W_IN_PROGRESS;
	}

	if (priv->msg_w_idx == priv->msgs_num)
		intr_mask = TX_EMPTY;

	if (priv->msg_err)
		intr_mask = ~0;

	if (intr_mask)
		rtk_i2c_int_disable(priv, intr_mask);

	rtk_i2c_check_intr_status(priv);
}


static void rtk_i2c_xfer_read(struct rtk_i2c_priv *priv)
{
	struct i2c_msg *msgs = priv->msgs;
	int rx_valid;

	for (; priv->msg_r_idx < priv->msgs_num; priv->msg_r_idx++) {
		unsigned int len;
		unsigned char *buf;

		if (!(msgs[priv->msg_r_idx].flags & I2C_M_RD))
			continue;

		if (!(priv->status & STATUS_R_IN_PROGRESS)) {
			len = msgs[priv->msg_r_idx].len;
			buf = msgs[priv->msg_r_idx].buf;
		} else {
			len = priv->rx_buf_len;
			buf = priv->rx_buf;
		}

		rx_valid = readl(priv->base + I2C_RXFLR);

		for (; len > 0 && rx_valid > 0; len--, rx_valid--) {
			*buf++ = readl(priv->base + I2C_DATA_CMD);
			priv->rx_outstanding--;
		}

		if (len > 0) {
			priv->status |= STATUS_R_IN_PROGRESS;
			priv->rx_buf_len = len;
			priv->rx_buf = buf;
			return;
		}
		priv->status &= ~STATUS_R_IN_PROGRESS;
	}
}

static int rtk_i2c_xfer(struct udevice *dev, struct i2c_msg *msg,
			    int nmsgs)
{
	unsigned int val;
	unsigned int addr = 0;
	int ret;
	struct rtk_i2c_priv *priv = dev_get_priv(dev);
	unsigned int status;

	priv->msgs = msg;
	priv->msgs_num = nmsgs;
	priv->msg_err = 0;
	priv->msg_w_idx = 0;
	priv->msg_r_idx = 0;
	priv->status = STATUS_IDLE;
	priv->rx_outstanding = 0;
	priv->abort_source = 0;

	ret = rtk_i2c_wait_bus_not_busy(priv);
	if (ret < 0) {
		priv->msg_err = -EIO;
		goto fail;
	}

	__rtk_i2c_disable(priv);

	val = readl(priv->base + I2C_CON);
	if (msg[priv->msg_w_idx].flags & I2C_M_TEN) {
		addr = msg[priv->msg_w_idx].addr & 0x3FF;
		writel(addr | TAR_TEN_BITADDR, priv->base + I2C_TAR);

		val |= TEN_BIT_MASTER | SLAVE_DISABLE | MASTER_EN | TX_EMPTY_CTL;
		writel(val, priv->base + I2C_CON);
	} else {
		addr = msg[priv->msg_w_idx].addr & 0x7F;
		writel(addr, priv->base + I2C_TAR);

		val = val & ~TEN_BIT_MASTER;
		val |= SLAVE_DISABLE | MASTER_EN | TX_EMPTY_CTL;
		writel(val, priv->base + I2C_CON);
	}

	rtk_i2c_int_disable(priv, ~0);

	writel(1, priv->base + I2C_ENABLE);

	readl(priv->base + I2C_ENABLE_STATUS);

	readl(priv->base + I2C_CLR_INT);

	rtk_i2c_int_enable(priv, INT_DEFAULT_MASK);

	rtk_i2c_check_intr_status(priv);

	if (priv->msg_w_idx != priv->msgs_num) {
		priv->msg_err = -EPROTO;
		rtk_i2c_init(priv);
		goto fail;
	}

	writel(0, priv->base + I2C_ENABLE);

	if (priv->msg_err)
		goto fail;

	return 0;
fail:
	if (priv->msg_err == -EPROTO)
		rtk_i2c_handle_tx_abort(priv);

	if (priv->msg_err == -ETIMEDOUT) {
		rtk_i2c_disable(priv);
		rtk_i2c_recover_bus(priv);
		rtk_i2c_init(priv);
		log_debug("reset module & reinit\n");
	}

	log_err("transmit error %d addr 0x%x msg_w_idx 0x%x nmsgs 0x%x\n",
		priv->msg_err, addr, priv->msg_w_idx, nmsgs);

	if (priv->msg_err)
		log_err("[%s] fail, msg_err: %d\n", __func__, priv->msg_err);
	return priv->msg_err;
}

static int rtk_i2c_probe(struct udevice *dev)
{
	int ret;
	uint32_t val;
	struct rtk_i2c_priv *priv = dev_get_priv(dev);

	ret = dev_read_alias_seq(dev, &priv->nr);
    if (ret < 0) {
        log_err("alias not found!\n");
		return -ENOENT;
	}

	log_info("[%s:%d] nr: %d\n", __func__, __LINE__, priv->nr);

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base) {
		log_err("reg base not found\n");
		return -ENOENT;
	}

	priv->irqbase = dev_read_addr_index(dev, 1);
    if (!priv->base) {
		log_err("reg irq not found\n");
		return -ENOENT;
	}

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret) {
		log_err("Invalid clk, id: %lu\n", priv->clk.id);
		return ret;
	}
	clk_enable(&priv->clk);

	rtk_i2c_reset_deassert(priv);

	rtk_i2c_set_pinmux(priv);

	priv->bus_freq_hz = I2C_SPEED_STANDARD_RATE;
	/* rtk i2c initial */
	ret = rtk_i2c_init(priv);
	if (ret)
		goto probe_fail;


probe_fail:
	if (ret)
		log_err("rtk i2c probe fail!\n");
	return 0;
}

static const struct dm_i2c_ops rtk_i2c_ops = {
	.xfer		= rtk_i2c_xfer,
	// .set_bus_speed	= rtk_i2c_set_speed,
};

static const struct udevice_id rtk_i2c_ids[] = {
	{ .compatible = "realtek,i2c"},
	{}
};

#if CONFIG_IS_ENABLED(DM_I2C)
U_BOOT_DRIVER(rtk_i2c_drv) = {
	.name		= "rtk i2c",
	.id			= UCLASS_I2C,
	.of_match	= rtk_i2c_ids,
	.probe		= rtk_i2c_probe,
	.priv_auto	= sizeof(struct rtk_i2c_priv),
	.ops		= &rtk_i2c_ops,
};
#endif