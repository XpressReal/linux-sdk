// SPDX-License-Identifier: GPL-2.0-only

#include <drm/drm_of.h>
#include <drm/drm_print.h>

#include <linux/component.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/pwm.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>

#include <video/of_display_timing.h>
#include <video/display_timing.h>

#include "rtk_drm_drv.h"
#include "rtk_dsi_reg.h"
#include "rtk_dsi.h"

ssize_t enable_dsi_pattern_gen_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR_WO(enable_dsi_pattern_gen);

struct rtk_dsi;

#define to_rtk_dsi(x) container_of(x, struct rtk_dsi, x)

struct rtk_dsi_data {
	unsigned int type;
	int (*enable)(struct drm_encoder *encoder);
	int (*disable)(struct drm_encoder *encoder);
	int (*query_timings)(struct device_node *np, struct rtk_dsi *dsi);
	void (*get_modes)(struct rtk_dsi *dsi);
};

struct rtk_dsi {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_panel *panel;

	struct drm_display_mode disp_mode;
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct mipi_dsi_host host;
	struct regmap *reg;
	struct clk *clk;
	struct reset_control *rstc;
	struct rtk_rpc_info *rpc_info;
	enum dsi_fmt fmt;
	struct mipi_dsi_device *device;
	unsigned int swap_enable;
	unsigned int lk_initialized;
	unsigned int mixer;

	const struct rtk_dsi_data *dsi_data;
	enum display_panel_usage display_panel_usage;
};

static struct regmap *dsi_reg;

static void rtk_dsi_init_raspberry_pi(struct rtk_dsi *dsi, struct drm_display_mode *mode)
{
	struct mipi_dsi_device *device = dsi->device;

	regmap_write(dsi->reg, CLOCK_GEN, 0x3f4000);
	regmap_write(dsi->reg, WATCHDOG, 0x1632);
	regmap_write(dsi->reg, CTRL_REG, 0x7000000);
	regmap_write(dsi->reg, DF, 0x1927c20);
	regmap_write(dsi->reg, SSC2, 0x5870587);
	regmap_write(dsi->reg, SSC3, 0x281515);
	regmap_write(dsi->reg, MPLL, 0x403592b);
	regmap_write(dsi->reg, TX_DATA1, 0x70d0100);
	regmap_write(dsi->reg, TX_DATA2, 0x81d090f);
	regmap_write(dsi->reg, TX_DATA3, 0x5091408);
	regmap_write(dsi->reg, CLOCK_GEN, 0x3f4710);
	regmap_write(dsi->reg, DF, 0x1927c3c);
	regmap_write(dsi->reg, WATCHDOG, 0x161a);
	regmap_write(dsi->reg, CLK_CONTINUE, 0x80);
	regmap_write(dsi->reg, TC5, 0x3200a90);
	regmap_write(dsi->reg, TC4, 0x2410552);
	regmap_write(dsi->reg, TO1, 0xffff);
	regmap_write(dsi->reg, TO2, 0xffff);
	regmap_write(dsi->reg, CTRL_REG, 0x7010000);

	if (device->format == MIPI_DSI_FMT_RGB888)
		regmap_write(dsi->reg, TC0,
			(mode->htotal - mode->hsync_end) << 16 | mode->hdisplay * 3);
	else if (device->format == MIPI_DSI_FMT_RGB565)
		regmap_write(dsi->reg, TC0,
			(mode->htotal - mode->hsync_end) << 16 | mode->hdisplay * 2);
	else if (device->format == MIPI_DSI_FMT_RGB666)
		regmap_write(dsi->reg, TC0,
			(mode->htotal - mode->hsync_end) << 16 | mode->hdisplay * 225 / 100);

	regmap_write(dsi->reg, TC2,
		(mode->vtotal - mode->vsync_end) << 16 | mode->vdisplay);
	regmap_write(dsi->reg, TC1,
		(mode->hsync_end - mode->hsync_start) << 16 | (mode->hsync_start - mode->hdisplay));
	regmap_write(dsi->reg, TC3,
		(mode->vsync_end - mode->vsync_start) << 16 | (mode->vsync_start - mode->vdisplay));
}

static void rtk_dsi_setup_lanes(struct rtk_dsi *dsi)
{
	struct mipi_dsi_device *device = dsi->device;
	unsigned int lane_enable;

	if (device->lanes == 4)
		lane_enable = LANE3_ENABLE | LANE2_ENABLE | LANE1_ENABLE | LANE0_ENABLE;
	else if (device->lanes == 2)
		lane_enable = LANE1_ENABLE | LANE0_ENABLE;
	else
		lane_enable = LANE0_ENABLE;

	regmap_update_bits(dsi->reg, CLOCK_GEN, CLOCL_GEN_11_8_MASK,
				TX_RESET | RX_RESET | LANE_CLK_ENABLE);

	regmap_update_bits(dsi->reg, CLOCK_GEN, CLOCL_GEN_LANE_ENABLE_MASK, lane_enable);
	regmap_update_bits(dsi->reg, CLOCK_GEN, CLOCL_GEN_3_0_MASK, 0);
}

static void rtk_dsi_setup_dphy(struct rtk_dsi *dsi,	unsigned int speed,
				unsigned int div_num, unsigned int pll, unsigned int time_period)
{
	unsigned int mpll;
	unsigned int lf;
	unsigned int reg_tx_lpx_time, reg_hs_exit_time, reg_tx_init_time;
	unsigned int reg_clk_pre_time, reg_clk_zero_time, reg_clk_prpr_time, reg_clk_post_time;
	unsigned int reg_tx_vld_time, reg_tx_tail_time, reg_hs_zero_time, reg_hs_prpr_time;
	unsigned int tmp, tmp1, tmp2, tmp3, tmp4;
	unsigned int tmp1_condition;
	unsigned int tx_data3_7_0_0, tx_data3_7_0_1;
	unsigned int tx_swap, rx_swap;

	// CLOCK_GEN[31:12] = 0x3F4 << 12
	// CLOCK_GEN[11:8] = 0 << 11 | TX_RESET << 10 | RX_RESET << 9 | LANE_CLK_ENABLE << 8
	// CLOCK_GEN[7:4] = LANE3_ENABLE << 7 | LANE2_ENABLE << 6 | LANE1_ENABLE << 5 | LANE0_ENABLE << 4
	// CLOCK_GEN[3:0] = 0
	regmap_write(dsi->reg, CLOCK_GEN, (0x3F4 << REG_CK_ESCAPE_DISABLE));
	regmap_write(dsi->reg, WATCHDOG, 0x1632);

	// CTRL_REG = 0x07000000 // DPHY/APHY reset
	regmap_write(dsi->reg, CTRL_REG, DPHY_EXTERNAL_RESET | APHY_RESET | APHY_CORE_POWER_DOWN);

	// MPLL = if speed > 800 -> 67328034 + (1 << 28) + (5 << 6) + 3 << 2 + 1
	// MPLL = if speed > 700 && speed <= 800 -> 67328034 + (2 << 28) + (5 << 6) + (3 << 2)
	// MPLL = if speed > 400 && speed <=700 -> 67328034 + (5 << 6) + (3 << 2)
	mpll = (speed > 800) ? 67328034 + (1 << 28) + (5 << 6) + (3 << 2) + 1 :
			((speed > 700 && speed <= 800) ? 67328034 + (2 << 28) + (5 << 6) + (3 << 2) :
				67328034 + (5 << 6) + (3 << 2));
	regmap_write(dsi->reg, MPLL, mpll);

	// TXF = 0xd88 (for every panel)
	regmap_write(dsi->reg, TXF, 0xD88);

    // LF = if LANE0_SEL == 4 -> 1*2^27+(1*2^26+1*2^25)
    // LF = if LANE1_SEL == 4 -> 1*2^28+(1*2^26+1*2^25)
    // LF = if LANE2_SEL == 4 -> 1*2^29+(1*2^26+1*2^25)
    // LF = if LANE3_SEL == 4 -> 1*2^30+(1*2^26+1*2^25)
    // LF = if LANE_CLK_SEL == 4 -> 1*2^31+(1*2^26+1*2^25)

	lf = (LANE0_SEL == 4) ? (1<<27)+(1<<26)+(1<<25) :
			((LANE1_SEL == 4) ? (1<<28)+(1<<26)+(1<<25) :
				(LANE2_SEL == 4) ? (1<<29)+(1<<26)+(1<<25) :
				(LANE3_SEL == 4) ? (1<<30)+(1<<26)+(1<<25) : (1<<31)+(1<<26)+(1<<25));
	regmap_write(dsi->reg, LF, lf);

	// DF = if div_num == 4 -> 0x0x01927c3e, if div_num == 2 -> 0x01927c3d, else -> 0x01927c3c
	if (div_num == 4)
		regmap_write(dsi->reg, DF, 0x01927c3e);
	else if (div_num == 2)
		regmap_write(dsi->reg, DF, 0x01927c3d);
	else
		regmap_write(dsi->reg, DF, 0x01927c3c);

	// TX_SWAP = LANE0_SEL + LANE1_SEL << 4 + LANE2_SEL << 8 + LANE3_SEL << 12 + LANE_CLK_SEL << 16 +
	//           LANE0_PN_SWAP << 20 + LANE1_PN_SWAP << 21 + LANE2_PN_SWAP << 22 + LANE3_PN_SWAP << 23 + LANE_CLK_PN_SWAP << 24
	// RX_SWAP = LANE0_SEL + LANE1_SEL << 4 + LANE2_SEL << 8 + LANE3_SEL << 12 + LANE_CLK_SEL << 16

	tx_swap = LANE0_SEL + (LANE1_SEL << 4) + (LANE2_SEL << 8) + (LANE3_SEL << 12) +
				(LANE_CLK_SEL << 16) + (LANE0_PN_SWAP << 20) + (LANE1_PN_SWAP << 21) +
				(LANE2_PN_SWAP << 22) + (LANE3_PN_SWAP << 23) + (LANE_CLK_PN_SWAP << 24);
	rx_swap = LANE0_SEL + (LANE1_SEL << 4) + (LANE2_SEL << 8) + (LANE3_SEL << 12) + (LANE_CLK_SEL << 16);
	regmap_write(dsi->reg, TX_SWAP, tx_swap);
	regmap_write(dsi->reg, RX_SWAP, rx_swap);

	// SSC3 = INT(PLL / XTAL) -3
	/* check value 0x282000 */
	regmap_write(dsi->reg, SSC3, 0x280000 | ((pll / XTAL_FREQ) - 3) << 8);

	// SSC2 = INT(((PLL / XTAL) - INT(PLL / XTAL)) * 2048)
	/* check fomula */
	regmap_write(dsi->reg, SSC2, ((((pll << FIXED_SHIFT) / XTAL_FREQ) - ((pll << FIXED_SHIFT) / XTAL_FREQ)) * 2048) >> FIXED_SHIFT); /* check value 0x282000 */


	// TX_DATA1[31:24] = INT(50 / time_period)
	reg_tx_lpx_time = (50 << FIXED_SHIFT) * 1 / time_period;

	// TX_DATA1[23:16] = INT((100 / time_period) + 1)
	reg_hs_exit_time = (100 << FIXED_SHIFT) * 1 / time_period + 1;

	// TX_DATA1[15:0] = 256
	reg_tx_init_time = 256;

	regmap_write(dsi->reg, TX_DATA1, (reg_tx_lpx_time << REG_TX_LPX_TIME) |
									(reg_hs_exit_time << REG_HS_EXIT_TIME) |
									(reg_tx_init_time << REG_TX_INIT_TIME));

	// TX_DATA2[31:24] = INT(time_period/8*8) + 1
	reg_clk_pre_time = (time_period / (8 << FIXED_SHIFT)) * 8 + 1;

	// TX_DATA2[23:16] = INT(300 / time_period) + 1
	reg_clk_zero_time = (300 << FIXED_SHIFT) * 1 / time_period + 1;

	// TX_DATA2[15:8] = if time_period > 40 -> INT(95 / time_period) - 1
	// 					if time_period <= 40 -> INT( 38 / time_period) + 1)
	reg_clk_prpr_time = ((time_period >> FIXED_SHIFT) > 40) ?
					(95 << FIXED_SHIFT) * 1 / time_period - 1 :
					(38 << FIXED_SHIFT) * 1 / time_period + 1;

	// TX_DATA2[7:0] = INT(((60 + 52 * time_period / 8) / time_period) + 1)
	tmp = 52 * time_period / (8 << 10) + 60;
	reg_clk_post_time = ((tmp << 10) / time_period) + 1;

	regmap_write(dsi->reg, TX_DATA2, (reg_clk_pre_time << REG_CLK_PRE_TIME) |
									(reg_clk_zero_time << REG_CLK_ZERO_TIME) |
									(reg_clk_prpr_time << REG_CLK_PRPR_TIME) |
									(reg_clk_post_time << REG_CLK_POST_TIME));

	// TX_DATA3[31:24] = 5
	reg_tx_vld_time = 5;

	// TX_DATA3[23:16] = if (INT((60 + (4 * time_period / 8)) / time_period) + 1) > 1 -> (INT((60 + (4 * time_period / 8)) / time_period) + 1)
	//                   if (INT((60 + (4 * time_period / 8)) / time_period) + 1) <=1 -> 1
	tmp1 = 4 * time_period / (8 << 10) + 60;
	tmp1_condition = ((tmp1 << 10) / time_period) + 1;
	reg_tx_tail_time = (tmp1_condition) > 1 ? tmp1_condition : 1;

	// TX_DATA3[15:8] = INT((145 + 10 * time_period / 8) / time_period) + 1
	tmp2 = 10 * time_period / (8 << 10) + 145;
	reg_hs_zero_time = ((tmp2 << 10) / time_period) + 1;

	// TX_DATA3[7:0] = if time_period > 40 -> INT((85 + 6 * time_period / 8) / time_period) - 1
	//				   if time_period <= 40 -> INT((40 + 4 * time_period / 8) /time_period) + 1)
	tmp3 = 6 * time_period / (8 << 10) + 85;
	tx_data3_7_0_0 = ((tmp3 << 10) / time_period) - 1;

	tmp4 = 4 * time_period / (8 << 10) + 40;
	tx_data3_7_0_1 = ((tmp4 << 10) / time_period) + 1;

	reg_hs_prpr_time = ((time_period >> FIXED_SHIFT) > 40) ?
					tx_data3_7_0_0 : tx_data3_7_0_1;

	regmap_write(dsi->reg, TX_DATA3, (reg_tx_vld_time << REG_TX_VLD_TIME) |
									(reg_tx_tail_time << REG_TX_TAIL_TIME) |
									(reg_hs_zero_time << REG_HS_ZERO_TIME) |
									(reg_hs_prpr_time << REG_HS_PRPR_TIME));
}

static void rtk_dsi_setup_timings(struct rtk_dsi *dsi, struct drm_display_mode *mode, unsigned int line_time)
{
	struct mipi_dsi_device *device = dsi->device;
	unsigned int sync_pulse_mode = 0;
	unsigned int burst_mode = 0;

	if (device->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
		sync_pulse_mode = 1;

	if (device->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
		burst_mode = 1;

	// TC5 = line_time + (hactive << 16)
	regmap_write(dsi->reg, TC5,	(mode->hdisplay) << 16 | line_time);

	// TC4 = 37748736 + (1(burst_mode) << 17) + 499,94(BLLP_LEN) + (1(sync_mode) << 16)
	/* 37748736=0x2400000=(TC4[22] | TC4[25]) */
	regmap_write(dsi->reg, TC4,	(2 << CRC_LEN) | (4 << HEADER_LEN) |
		(line_time / 2) | (sync_pulse_mode << SYNC_PULSE_MODE) | (burst_mode << BURST_MODE));

	if (device->format == MIPI_DSI_FMT_RGB888)
		regmap_write(dsi->reg, TC0,
			(mode->htotal - mode->hsync_end) << 16 | mode->hdisplay * 3);
	else if (device->format == MIPI_DSI_FMT_RGB565)
		regmap_write(dsi->reg, TC0,
			(mode->htotal - mode->hsync_end) << 16 | mode->hdisplay * 2);
	else if (device->format == MIPI_DSI_FMT_RGB666)
		regmap_write(dsi->reg, TC0,
			(mode->htotal - mode->hsync_end) << 16 | mode->hdisplay * 225 / 100);

	regmap_write(dsi->reg, TC2,
		(mode->vtotal - mode->vsync_end) << 16 | mode->vdisplay);
	regmap_write(dsi->reg, TC1,
		(mode->hsync_end - mode->hsync_start) << 16 | (mode->hsync_start - mode->hdisplay));
	regmap_write(dsi->reg, TC3,
		(mode->vsync_end - mode->vsync_start) << 16 | (mode->vsync_start - mode->vdisplay));
}

static void rtk_dsi_init(struct rtk_dsi *dsi, struct drm_display_mode *mode)
{
	struct mipi_dsi_device *device = dsi->device;
	unsigned int data_rate;
	unsigned int speed;
	unsigned int frame_rate;
	unsigned int div_num;
	unsigned int pll;
	unsigned int line_time;
	unsigned int time_period;

	if (!strcmp(device->name, "rpi-ts-dsi")) {
		dev_info(dsi->dev, "Init raspberry pi 7 inch touch panel\n");
		rtk_dsi_init_raspberry_pi(dsi, mode);
		return;
	}

	frame_rate = drm_mode_vrefresh(mode);
	data_rate = mode->htotal * mode->vtotal * frame_rate * 24 / TOTAL_LANE_NUM;
	speed = DIV_ROUND_UP(data_rate, MHZ(27))  * XTAL_FREQ;
	div_num = (speed < 200) ? 4 : ((speed > 400) ? 1 : 2);
	pll = speed * div_num;
	line_time = (speed * MHZ(1)) / (frame_rate * mode->vtotal * 8);
	/* 1000 / (speed / 8); */
	time_period = (1000 << FIXED_SHIFT) * 8 / speed;

	DRM_DEBUG_DRIVER("data_rate   = %d\n", data_rate);
	DRM_DEBUG_DRIVER("speed       = %d\n", speed);
	DRM_DEBUG_DRIVER("div_num     = %d\n", div_num);
	DRM_DEBUG_DRIVER("pll         = %d\n", pll);
	DRM_DEBUG_DRIVER("line_time   = %d\n", line_time);
	DRM_DEBUG_DRIVER("time_period = %d\n", time_period);

	rtk_dsi_setup_dphy(dsi, speed, div_num, pll, time_period);
	rtk_dsi_setup_timings(dsi, mode, line_time);
	rtk_dsi_setup_lanes(dsi);

	regmap_write(dsi->reg, WATCHDOG, 0x161a);
	regmap_write(dsi->reg, CLK_CONTINUE, 0x80);

	return;
}

static int rtk_dsi_enable(struct drm_encoder *encoder)
{
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	struct rtk_dsi *dsi = to_rtk_dsi(encoder);
	struct rtk_rpc_info *rpc_info = dsi->rpc_info;
	struct mipi_dsi_device *device = dsi->device;
	struct rpc_set_display_out_interface interface;
	int ret;

	ret = clk_prepare_enable(dsi->clk);
	if (ret)
		DRM_ERROR("Failed to enable clk: %d\n", ret);

	reset_control_deassert(dsi->rstc);

	rtk_dsi_init(dsi, mode);

	if (dsi->panel)
		drm_panel_prepare(dsi->panel);

	regmap_update_bits(dsi->reg, CTRL_REG, VIDEO_MODE, VIDEO_MODE);

	if (device->lanes == 4)
		regmap_update_bits(dsi->reg, CTRL_REG, LANE_NUM_MASK, LANE_NUM_4);
	else if (device->lanes == 2)
		regmap_update_bits(dsi->reg, CTRL_REG, LANE_NUM_MASK, LANE_NUM_2);
	else
		regmap_update_bits(dsi->reg, CTRL_REG, LANE_NUM_MASK, LANE_NUM_1);

	regmap_update_bits(dsi->reg, CTRL_REG, EOTP_EN, EOTP_EN);
	regmap_update_bits(dsi->reg, CTRL_REG,
					RX_ECC_EN | RX_CRC_EN, RX_ECC_EN | RX_CRC_EN);

	regmap_write(dsi->reg, PAT_GEN, 0x9000000);

	interface.display_interface       = DISPLAY_INTERFACE_MIPI;
	interface.width                   = mode->hdisplay;
	interface.height                  = mode->vdisplay;
	interface.frame_rate              = drm_mode_vrefresh(mode);
	interface.display_interface_mixer = DISPLAY_INTERFACE_MIXER2;

	dev_info(dsi->dev, "enable %s %dx%d@%d on %s\n",
		interface_names[DISPLAY_INTERFACE_MIPI],
		mode->hdisplay, mode->vdisplay, drm_mode_vrefresh(mode),
		mixer_names[DISPLAY_INTERFACE_MIXER2]);

	ret = rpc_set_out_interface(rpc_info, &interface);
	if (ret)
		DRM_ERROR("rpc_set_out_interface rpc fail\n");

	if (dsi->panel)
		drm_panel_enable(dsi->panel);

	return 0;
}

static int rtk_car_dsi_enable(struct drm_encoder *encoder)
{
	struct rtk_dsi *dsi = to_rtk_dsi(encoder);
	struct rtk_rpc_info *rpc_info = dsi->rpc_info;
	struct rpc_hw_init_display_out_interface hw_init_rpc;
	int ret;

	hw_init_rpc.display_interface = DISPLAY_INTERFACE_MIPI;
	hw_init_rpc.enable = 1;

	dev_info(dsi->dev, "enable interface %s\n",
		interface_names[DISPLAY_INTERFACE_MIPI]);

	ret = rpc_hw_init_out_interface(rpc_info, &hw_init_rpc);
	if (ret)
		DRM_ERROR("rpc_hw_init_out_interface rpc fail\n");

	return 0;
}

static int rtk_dsi_disable(struct drm_encoder *encoder)
{
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	struct rtk_dsi *dsi = to_rtk_dsi(encoder);
	struct rtk_rpc_info *rpc_info = dsi->rpc_info;
	struct rpc_set_display_out_interface interface;
	int ret;

	interface.display_interface       = DISPLAY_INTERFACE_MIPI;
	interface.width                   = mode->hdisplay;
	interface.height                  = mode->vdisplay;
	interface.frame_rate              = drm_mode_vrefresh(mode);
	interface.display_interface_mixer = DISPLAY_INTERFACE_MIXER_NONE;

	regmap_write(dsi->reg, PAT_GEN, 0x9000000);

	dev_info(dsi->dev, "disable %s %dx%d@%d\n",
		interface_names[interface.display_interface],
		mode->hdisplay, mode->vdisplay, drm_mode_vrefresh(mode));

	ret = rpc_set_out_interface(rpc_info, &interface);
	if (ret) {
		DRM_ERROR("rpc_set_out_interface rpc fail\n");
		return -1;
	}

	if (dsi->panel) {
		drm_panel_disable(dsi->panel);
		drm_panel_unprepare(dsi->panel);
	}

	clk_disable_unprepare(dsi->clk);
	reset_control_assert(dsi->rstc);

	return 0;
}

static int rtk_car_dsi_disable(struct drm_encoder *encoder)
{
	struct rtk_dsi *dsi = to_rtk_dsi(encoder);
	struct rtk_rpc_info *rpc_info = dsi->rpc_info;
	struct rpc_hw_init_display_out_interface hw_init_rpc;
	int ret;

	hw_init_rpc.display_interface = DISPLAY_INTERFACE_MIPI;
	hw_init_rpc.enable = 0;

	dev_info(dsi->dev, "disable interface %s\n",
		interface_names[DISPLAY_INTERFACE_MIPI]);

	ret = rpc_hw_init_out_interface(rpc_info, &hw_init_rpc);
	if (ret)
		DRM_ERROR("rpc_hw_init_out_interface rpc fail\n");

	return 0;
}

static int get_display_panel_usage_by_mixer(struct rtk_dsi *dsi)
{
	struct rtk_rpc_info *rpc_info = dsi->rpc_info;
	struct rpc_query_display_panel_usage panel_usage;
	int ret = 0;

	panel_usage.display_interface_mixer = dsi->mixer;

	ret = rpc_query_display_panel_usage(rpc_info, &panel_usage);
	if (ret) {
		DRM_ERROR("rpc_query_display_panel_usage RPC fail\n");
		return -1;
	}

	dsi->display_panel_usage = panel_usage.display_panel_usage;

	dev_info(dsi->dev, "%s is for %d\n",
		interface_names[DISPLAY_INTERFACE_MIPI], dsi->display_panel_usage);

	return 0;
}

static void rtk_car_dsi_get_modes(struct rtk_dsi *dsi)
{
	struct drm_display_mode *disp_mode;
	struct rpc_query_panel_cluster_size cluster_size;

	disp_mode = &dsi->disp_mode;

	get_display_panel_usage_by_mixer(dsi);

	if (dsi->display_panel_usage == CLUSTER) {
		rpc_query_panel_cluster_size(dsi->rpc_info, &cluster_size);

		dev_info(dsi->dev, "cluster size is (%d, %d)\n",
			cluster_size.width, cluster_size.height);

		disp_mode->hdisplay = cluster_size.width;
		disp_mode->vdisplay = cluster_size.height;
	}
}

static int rtk_car_dsi_query_timings(struct device_node *np, struct rtk_dsi *dsi)
{
	struct rtk_rpc_info *rpc_info = dsi->rpc_info;
	struct drm_display_mode *disp_mode;
	struct rpc_query_display_out_interface_timing interface_timing;
	int ret = 0;

	interface_timing.display_interface = DISPLAY_INTERFACE_MIPI;

	disp_mode = &dsi->disp_mode;

	rpc_query_out_interface_timing(rpc_info, &interface_timing);
	disp_mode->clock       = interface_timing.clock;
	disp_mode->hdisplay    = interface_timing.hdisplay;
	disp_mode->hsync_start = interface_timing.hsync_start;
	disp_mode->hsync_end   = interface_timing.hsync_end;
	disp_mode->htotal      = interface_timing.htotal;
	disp_mode->vdisplay    = interface_timing.vdisplay;
	disp_mode->vsync_start = interface_timing.vsync_start;
	disp_mode->vsync_end   = interface_timing.vsync_end;
	disp_mode->vtotal      = interface_timing.vtotal;
	disp_mode->flags       = 0;

	dsi->mixer = interface_timing.mixer;

	dev_info(dsi->dev, "[dsi][rpc_query_out_interface_timing] (%dx%d)@%d on %s\n",
		disp_mode->hdisplay, disp_mode->vdisplay,
		drm_mode_vrefresh(disp_mode), mixer_names[dsi->mixer]);

	return ret;
}

static void rtk_dsi_enc_mode_set(struct drm_encoder *encoder,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adj_mode)
{
	struct rtk_dsi *dsi = to_rtk_dsi(encoder);

	if (adj_mode->hdisplay == 1280 &&
		adj_mode->vdisplay == 720 &&
		drm_mode_vrefresh(adj_mode) == 60) {
		dsi->fmt = DSI_FMT_720P_60;
	} else if (adj_mode->hdisplay == 1920 &&
		adj_mode->vdisplay == 1080 &&
		drm_mode_vrefresh(adj_mode) == 60) {
		dsi->fmt = DSI_FMT_1080P_60;
	} else if (adj_mode->hdisplay == 1200 &&
		adj_mode->vdisplay == 1920 &&
		drm_mode_vrefresh(adj_mode) == 60) {
		dsi->fmt = DSI_FMT_1200_1920P_60;
	} else if (adj_mode->hdisplay == 800 &&
		adj_mode->vdisplay == 1280 &&
		drm_mode_vrefresh(adj_mode) == 60) {
		dsi->fmt = DSI_FMT_800_1280P_60;
	} else if (adj_mode->hdisplay == 600 &&
		adj_mode->vdisplay == 1024 &&
		drm_mode_vrefresh(adj_mode) == 60) {
		dsi->fmt = DSI_FMT_600_1024P_60;
	} else if (adj_mode->hdisplay == 1920 &&
		adj_mode->vdisplay == 720 &&
		drm_mode_vrefresh(adj_mode) == 60) {
		dsi->fmt = DSI_FMT_1920_720P_60;
	} else if (adj_mode->hdisplay == 1920 &&
		adj_mode->vdisplay == 720 &&
		drm_mode_vrefresh(adj_mode) == 30) {
		dsi->fmt = DSI_FMT_1920_720P_30;
	} else if (adj_mode->hdisplay == 600 &&
		adj_mode->vdisplay == 1024 &&
		drm_mode_vrefresh(adj_mode) == 30) {
		dsi->fmt = DSI_FMT_600_1024P_30;
	} else if (adj_mode->hdisplay == 800 &&
		adj_mode->vdisplay == 480 &&
		drm_mode_vrefresh(adj_mode) == 60) {
		dsi->fmt = DSI_FMT_800_480P_60;
	}

	dev_info(dsi->dev, "dsi->fmt (%d)\n", dsi->fmt);
}

static void rtk_dsi_enc_enable(struct drm_encoder *encoder)
{
	struct rtk_dsi *dsi = to_rtk_dsi(encoder);
	int ret;

	ret = dsi->dsi_data->enable(encoder);
	if (ret) {
		DRM_ERROR("dsi enc enable failed\n");
		return;
	}
}

static void rtk_dsi_enc_disable(struct drm_encoder *encoder)
{
	struct rtk_dsi *dsi = to_rtk_dsi(encoder);
	int ret;

	ret = dsi->dsi_data->disable(encoder);
	if (ret) {
		DRM_ERROR("dsi enc disable failed\n");
		return;
	}
}

static int rtk_dsi_enc_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	return 0;
}

ssize_t enable_dsi_pattern_gen_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long state;
	int ret, enable, pat_gen;

	ret = kstrtol(buf, 0, &state);
	/* valid input value: 0x8-0xf */
	enable = state & (DSI_PAT_GEN_MAX);

	if (!enable) {
		regmap_write(dsi_reg, PAT_GEN, 0x0);
		return count;
	}

	pat_gen = state & (0x7);

	switch (pat_gen) {
	case DSI_PAT_GEN_COLORBAR:
		regmap_write(dsi_reg, PAT_GEN, 0x8000000);
		break;
	case DSI_PAT_GEN_BLACK:
		regmap_write(dsi_reg, PAT_GEN, 0x9000000);
		break;
	case DSI_PAT_GEN_WHITE:
		regmap_write(dsi_reg, PAT_GEN, 0xa000000);
		break;
	case DSI_PAT_GEN_RED:
		regmap_write(dsi_reg, PAT_GEN, 0xb000000);
		break;
	case DSI_PAT_GEN_BLUE:
		regmap_write(dsi_reg, PAT_GEN, 0xc000000);
		break;
	case DSI_PAT_GEN_YELLOW:
		regmap_write(dsi_reg, PAT_GEN, 0xd000000);
		break;
	case DSI_PAT_GEN_MAGENTA:
		regmap_write(dsi_reg, PAT_GEN, 0xe000000);
		break;
	case DSI_PAT_GEN_USER_DEFINE:
		regmap_write(dsi_reg, PAT_GEN, 0xf000000);
		break;
	default:
		DRM_ERROR("Invalid argument\n");
		break;
	}

	return count;
}

static const struct drm_encoder_funcs rtk_dsi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs rtk_dsi_encoder_helper_funcs = {
	.mode_set     = rtk_dsi_enc_mode_set,
	.enable       = rtk_dsi_enc_enable,
	.disable      = rtk_dsi_enc_disable,
	.atomic_check = rtk_dsi_enc_atomic_check,
};

static enum drm_connector_status rtk_dsi_conn_detect(
	struct drm_connector *connector, bool force)
{
	struct rtk_dsi *dsi = to_rtk_dsi(connector);
	enum drm_connector_status status = connector_status_disconnected;

	if (dsi->dsi_data->type == RTK_AUTOMOTIVE_TYPE)
		return connector_status_connected;

	if (dsi->panel)
		status = connector_status_connected;

	return status;
}

static void rtk_dsi_conn_destroy(struct drm_connector *connector)
{
	drm_connector_cleanup(connector);
}

static int rtk_dsi_conn_get_modes(struct drm_connector *connector)
{
	struct rtk_dsi *dsi = to_rtk_dsi(connector);
	struct drm_display_mode *mode;

	if (dsi->dsi_data->type == RTK_NORMAL_TYPE)
		return drm_panel_get_modes(dsi->panel, connector);

	dsi->dsi_data->get_modes(dsi);

	mode = drm_mode_duplicate(connector->dev, &dsi->disp_mode);
	drm_mode_set_name(mode);

	if (!mode) {
		DRM_ERROR("bad mode or failed to add mode\n");
		return -EINVAL;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	connector->display_info.width_mm  = dsi->disp_mode.width_mm;
	connector->display_info.height_mm = dsi->disp_mode.height_mm;

	drm_mode_probed_add(connector, mode);

	return 1;
}

static enum drm_mode_status rtk_dsi_conn_mode_valid(
	struct drm_connector *connector, struct drm_display_mode *mode)
{
	return MODE_OK;
}

static const struct drm_connector_funcs rtk_dsi_connector_funcs = {
	.fill_modes             = drm_helper_probe_single_connector_modes,
	.detect                 = rtk_dsi_conn_detect,
	.destroy                = rtk_dsi_conn_destroy,
	.reset                  = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state   = drm_atomic_helper_connector_destroy_state,
};

static struct drm_connector_helper_funcs rtk_dsi_connector_helper_funcs = {
	.get_modes  = rtk_dsi_conn_get_modes,
	.mode_valid = rtk_dsi_conn_mode_valid,
};

static int rtk_dsi_bind(struct device *dev, struct device *master,
				 void *data)
{
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_dsi *dsi = dev_get_drvdata(dev);
	int ret;
	int err = 0;

	dsi->clk = devm_clk_get(dev, "clk_en_dsi");
	if (IS_ERR(dsi->clk)) {
		dev_err(dev, "failed to get clock\n");
		return PTR_ERR(dsi->clk);
	}

	ret = clk_prepare_enable(dsi->clk);
	if (ret) {
		DRM_ERROR("Failed to enable clk: %d\n", ret);
		return ret;
	}

	dsi->rstc = devm_reset_control_get(dev, "dsi");
	if (IS_ERR(dsi->rstc)) {
		dev_err(dev, "failed to get reset controller\n");
		return PTR_ERR(dsi->rstc);
	}
	reset_control_deassert(dsi->rstc);

	dsi->reg = syscon_regmap_lookup_by_phandle(dev->of_node, "syscon");
	if (IS_ERR(dsi->reg))
		return PTR_ERR(dsi->reg);

	dsi->drm_dev = drm;
	dsi_reg = dsi->reg;

	of_property_read_u32(dev->of_node, "lk-init", &dsi->lk_initialized);

	dsi->rpc_info = &priv->rpc_info[RTK_RPC_MAIN];
	dev_info(dev, "dsi->rpc_info (%p)\n", dsi->rpc_info);

	if (dsi->dsi_data->query_timings) {
		ret = dsi->dsi_data->query_timings(dev->of_node, dsi);
		if (ret) {
			DRM_ERROR("dsi query timings failed\n");
			return -EINVAL;
		}
	}

	encoder = &dsi->encoder;
	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;

	dev_info(dev, "dsi possible_crtcs (0x%x)\n", encoder->possible_crtcs);

	drm_encoder_init(drm, encoder, &rtk_dsi_encoder_funcs,
			 DRM_MODE_ENCODER_DSI, NULL);

	drm_encoder_helper_add(encoder, &rtk_dsi_encoder_helper_funcs);

	connector = &dsi->connector;
	drm_connector_init(drm, connector, &rtk_dsi_connector_funcs,
			   DRM_MODE_CONNECTOR_DSI);
	drm_connector_helper_add(connector, &rtk_dsi_connector_helper_funcs);

	drm_connector_attach_encoder(connector, encoder);

	err = device_create_file(drm->dev, &dev_attr_enable_dsi_pattern_gen);
	if (err < 0)
		DRM_ERROR("failed to create dsi pattern gen\n");

	return 0;
}

static void rtk_dsi_unbind(struct device *dev, struct device *master,
			     void *data)
{
	struct drm_device *drm = data;

	dsi_reg = NULL;

	device_remove_file(drm->dev, &dev_attr_enable_dsi_pattern_gen);
}

static const struct component_ops rtk_dsi_ops = {
	.bind	= rtk_dsi_bind,
	.unbind	= rtk_dsi_unbind,
};

static int rtk_dsi_host_attach(struct mipi_dsi_host *host,
			       struct mipi_dsi_device *device)
{
	struct rtk_dsi *dsi = to_rtk_dsi(host);
	struct drm_panel *panel;
	struct device *dev = host->dev;
	int ret;

	ret = drm_of_find_panel_or_bridge(dev->of_node, 1, 0, &panel, NULL);
	if (ret) {
		DRM_ERROR("Failed to find panel\n");
		return ret;
	}

	of_property_read_u32(dev->of_node, "swap", &dsi->swap_enable);

	dsi->panel = panel;
	dsi->device = device;

	dev_info(dev, "panel(%px)(%d lane) %s attached\n",
		dsi->panel, device->lanes, device->name);

	return ret;
}

static int rtk_dsi_host_detach(struct mipi_dsi_host *host,
			       struct mipi_dsi_device *device)
{
	struct rtk_dsi *dsi = to_rtk_dsi(host);

	dsi->panel = NULL;

	return 0;
}

static ssize_t rtk_dsi_host_transfer(struct mipi_dsi_host *host,
				     const struct mipi_dsi_msg *msg)
{
	struct rtk_dsi *dsi = to_rtk_dsi(host);
	unsigned char *buf;
	unsigned int len;
	unsigned int cnt, data[2], lastbyte;
	int i, j, tmp, ret = -1;
	unsigned int cmd;

	buf = (unsigned char *)msg->tx_buf;
	len = msg->tx_len;

	DRM_DEBUG_DRIVER("msg->type : 0x%x\n", msg->type);

	regmap_write(dsi->reg, TO1, 0xffff);
	regmap_write(dsi->reg, TO2, 0xffff);
	regmap_write(dsi->reg, CTRL_REG, 0x7010000);

	cmd = msg->type;
	switch (msg->type) {
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
		DRM_DEBUG_DRIVER("cmd : %d, len : %d\n", cmd, len);

		tmp = (buf[0] << 8) | cmd;

		if (len > 1)
			tmp |= buf[1] << 16;

		// pkt |= (((u8 *)msg->tx_buf)[0] << 8);
		// if (msg->tx_len > 1)
		// 	pkt |= (((u8 *)msg->tx_buf)[1] << 16);

		regmap_write(dsi->reg, CMD0, tmp);
		DRM_DEBUG_DRIVER("write CMD0 0x%x\n", tmp);
		break;
	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
		DRM_DEBUG_DRIVER("cmd : %d, len : %d\n", cmd, len);

		cnt = (len>>3) + ((len%8)?1:0);
		for (i = 0; i < cnt; i++) {
			data[0] = data[1] = 0;
			if (i == cnt-1)
				lastbyte = len % 8;
			else
				lastbyte = 8;

			for (j = 0; j < lastbyte; j++)
				*((unsigned char *)(data) + j) = buf[i*8+j];

			DRM_DEBUG_DRIVER("write IDMA1 0x%x\n", data[0]);
			DRM_DEBUG_DRIVER("write IDMA2 0x%x\n", data[1]);
			DRM_DEBUG_DRIVER("write IDMA0 0x%x\n", 0x10000 | i);

			regmap_write(dsi->reg, IDMA1, data[0]);
			regmap_write(dsi->reg, IDMA2, data[1]);
			regmap_write(dsi->reg, IDMA0, (0x10000 | i));
		}

		DRM_DEBUG_DRIVER("write CMD0 0x%x\n", (cmd | len << 8));
		regmap_write(dsi->reg, CMD0, (cmd | len << 8));

		break;
	default:
		pr_err("not support yet\n");
		break;
	}

	cnt = 0;
	regmap_write(dsi->reg, CMD_GO, 0x1);
	while (1) {
		regmap_read(dsi->reg, INTS, &tmp);
		tmp = tmp & 0x4;
		if (tmp || cnt >= 10)
			break;
		usleep_range(10000, 12000);
		cnt++;
	}
	if (cnt >= 10)
		dev_err(dsi->host.dev, "command fail\n");
	else
		ret = 0;

	regmap_write(dsi->reg, INTS, 0x4);

	return ret;
}

static const struct mipi_dsi_host_ops rtk_dsi_host_ops = {
	.attach = rtk_dsi_host_attach,
	.detach = rtk_dsi_host_detach,
	.transfer = rtk_dsi_host_transfer,
};

static const struct rtk_dsi_data rtk_normal_dsi_data = {
	.type = RTK_NORMAL_TYPE,
	.enable = rtk_dsi_enable,
	.disable = rtk_dsi_disable,
	.query_timings = NULL,
	.get_modes = NULL,
};

static const struct rtk_dsi_data rtk_car_dsi_data = {
	.type = RTK_AUTOMOTIVE_TYPE,
	.enable = rtk_car_dsi_enable,
	.disable = rtk_car_dsi_disable,
	.query_timings = rtk_car_dsi_query_timings,
	.get_modes = rtk_car_dsi_get_modes,
};

static int rtk_dsi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_dsi *dsi;

	dsi = devm_kzalloc(dev, sizeof(*dsi), GFP_KERNEL);
	if (!dsi)
		return -ENOMEM;

	dev_set_drvdata(dev, dsi);
	dsi->dev = dev;

	dsi->dsi_data = of_device_get_match_data(dev);
	if (!dsi->dsi_data)
		return -EINVAL;

	dsi->host.ops = &rtk_dsi_host_ops;
	dsi->host.dev = dev;
	mipi_dsi_host_register(&dsi->host);

	return component_add(&pdev->dev, &rtk_dsi_ops);
}

static int rtk_dsi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rtk_dsi_ops);

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int rtk_dsi_suspend(struct device *dev)
{
	struct rtk_dsi *dsi = dev_get_drvdata(dev);

	if (!dsi)
		return 0;

	return 0;
}

static int rtk_dsi_resume(struct device *dev)
{
	struct rtk_dsi *dsi = dev_get_drvdata(dev);

	if (!dsi)
		return 0;

	return 0;
}

static const struct dev_pm_ops rtk_dsi_pm_ops = {
	.suspend = rtk_dsi_suspend,
	.resume  = rtk_dsi_resume,
};
#endif

static const struct of_device_id rtk_dsi_dt_ids[] = {
	{
		.compatible = "realtek,rtk-dsi",
		.data = &rtk_normal_dsi_data
	},
	{
		.compatible = "realtek,rtk-car-dsi",
		.data = &rtk_car_dsi_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_dsi_dt_ids);

struct platform_driver rtk_dsi_driver = {
	.probe  = rtk_dsi_probe,
	.remove = rtk_dsi_remove,
	.driver = {
		.name = "rtk-dsi",
		.of_match_table = rtk_dsi_dt_ids,
#if IS_ENABLED(CONFIG_PM)
		.pm = &rtk_dsi_pm_ops,
#endif
	},
};

MODULE_AUTHOR("Ray Tang <ray.tang@realtek.com>");
MODULE_DESCRIPTION("Realtek MIPI DSI Driver");
MODULE_LICENSE("GPL v2");
