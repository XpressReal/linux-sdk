// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_mipi_csi.h"

#define DEFAULT_PHY_CHECK_MS   5000
#define DEFAULT_TUNING_SAMPLE  MAX_RTUNE

static int phy_check_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	struct rtk_mipicsi_debug *debug = &mipicsi->debug;

	if (val == 0)
		debug->phy_check_ms = DEFAULT_PHY_CHECK_MS;
	else
		debug->phy_check_ms = val;

	return 0;
}

static int phy_check_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	struct rtk_mipicsi_debug *debug = &mipicsi->debug;
	bool check_ok;

	if (mipicsi->ch_index >= CH_4)
		check_ok = mipicsi->hw_ops->phy_check(mipicsi, MIPI_TOP_1,
			debug->phy_check_ms);
	else
		check_ok = mipicsi->hw_ops->phy_check(mipicsi, MIPI_TOP_0,
			debug->phy_check_ms);

	*val = (u64)check_ok;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_phy_check_fops, phy_check_get, phy_check_set,
			 "%llu\n");

static int eq_tuning_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	struct rtk_mipicsi_debug *debug = &mipicsi->debug;

	if (val >= MAX_RTUNE)
		debug->tuning_sample = MAX_RTUNE;
	else if (val == 0)
		debug->tuning_sample = DEFAULT_TUNING_SAMPLE;
	else
		debug->tuning_sample = (u8)val;

	return 0;
}

static int eq_tuning_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	struct rtk_mipicsi_debug *debug = &mipicsi->debug;
	bool tuning_ok;

	if (mipicsi->ch_index >= CH_4)
		tuning_ok = mipicsi->hw_ops->eq_tuning(mipicsi, MIPI_TOP_1,
			debug->phy_check_ms, debug->tuning_sample);
	else
		tuning_ok = mipicsi->hw_ops->eq_tuning(mipicsi, MIPI_TOP_0,
			debug->phy_check_ms, debug->tuning_sample);

	*val = (u64)tuning_ok;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_eq_tuning_fops, eq_tuning_get, eq_tuning_set,
			 "%llu\n");

static int en_manual_deskew_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 manual_skw;

	manual_skw = val ? 1:0;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_manual_skw(mipicsi, MIPI_TOP_1, manual_skw);
	else
		mipicsi->hw_ops->aphy_set_manual_skw(mipicsi, MIPI_TOP_0, manual_skw);

	return 0;
}

static int en_manual_deskew_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 manual_skw;

	if (mipicsi->ch_index >= CH_4)
		manual_skw = mipicsi->hw_ops->aphy_get_manual_skw(mipicsi, MIPI_TOP_1);
	else
		manual_skw = mipicsi->hw_ops->aphy_get_manual_skw(mipicsi, MIPI_TOP_0);

	*val = (u64)manual_skw;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_en_manual_deskew_fops,
		en_manual_deskew_get, en_manual_deskew_set, "%llu\n");

static int aphy_skw_sclk0_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sclk0;

	sclk0 = (u8)val;

	if (sclk0 > MAX_SCLK)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_skw_sclk(mipicsi, MIPI_TOP_1, LANE_0, sclk0);
	else
		mipicsi->hw_ops->aphy_set_skw_sclk(mipicsi, MIPI_TOP_0, LANE_0, sclk0);

exit:
	return 0;
}

static int aphy_skw_sclk0_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sclk0;

	if (mipicsi->ch_index >= CH_4)
		sclk0 = mipicsi->hw_ops->aphy_get_skw_sclk(mipicsi, MIPI_TOP_1, LANE_0);
	else
		sclk0 = mipicsi->hw_ops->aphy_get_skw_sclk(mipicsi, MIPI_TOP_0, LANE_0);

	*val = (u64)sclk0;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_skw_sclk0_fops,
		aphy_skw_sclk0_get, aphy_skw_sclk0_set, "%llu\n");

static int aphy_skw_sclk1_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sclk1;

	sclk1 = (u8)val;

	if (sclk1 > MAX_SCLK)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_skw_sclk(mipicsi, MIPI_TOP_1, LANE_1, sclk1);
	else
		mipicsi->hw_ops->aphy_set_skw_sclk(mipicsi, MIPI_TOP_0, LANE_1, sclk1);

exit:
	return 0;
}

static int aphy_skw_sclk1_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sclk1;

	if (mipicsi->ch_index >= CH_4)
		sclk1 = mipicsi->hw_ops->aphy_get_skw_sclk(mipicsi, MIPI_TOP_1, LANE_1);
	else
		sclk1 = mipicsi->hw_ops->aphy_get_skw_sclk(mipicsi, MIPI_TOP_0, LANE_1);

	*val = (u64)sclk1;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_skw_sclk1_fops,
		aphy_skw_sclk1_get, aphy_skw_sclk1_set, "%llu\n");

static int aphy_skw_sclk2_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sclk2;

	sclk2 = (u8)val;

	if (sclk2 > MAX_SCLK)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_skw_sclk(mipicsi, MIPI_TOP_1, LANE_2, sclk2);
	else
		mipicsi->hw_ops->aphy_set_skw_sclk(mipicsi, MIPI_TOP_0, LANE_2, sclk2);

exit:
	return 0;
}

static int aphy_skw_sclk2_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sclk2;

	if (mipicsi->ch_index >= CH_4)
		sclk2 = mipicsi->hw_ops->aphy_get_skw_sclk(mipicsi, MIPI_TOP_1, LANE_2);
	else
		sclk2 = mipicsi->hw_ops->aphy_get_skw_sclk(mipicsi, MIPI_TOP_0, LANE_2);

	*val = (u64)sclk2;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_skw_sclk2_fops,
		aphy_skw_sclk2_get, aphy_skw_sclk2_set, "%llu\n");

static int aphy_skw_sclk3_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sclk3;

	sclk3 = (u8)val;

	if (sclk3 > MAX_SCLK)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_skw_sclk(mipicsi, MIPI_TOP_1, LANE_3, sclk3);
	else
		mipicsi->hw_ops->aphy_set_skw_sclk(mipicsi, MIPI_TOP_0, LANE_3, sclk3);

exit:
	return 0;
}

static int aphy_skw_sclk3_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sclk3;

	if (mipicsi->ch_index >= CH_4)
		sclk3 = mipicsi->hw_ops->aphy_get_skw_sclk(mipicsi, MIPI_TOP_1, LANE_3);
	else
		sclk3 = mipicsi->hw_ops->aphy_get_skw_sclk(mipicsi, MIPI_TOP_0, LANE_3);

	*val = (u64)sclk3;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_skw_sclk3_fops,
		aphy_skw_sclk3_get, aphy_skw_sclk3_set, "%llu\n");

static int aphy_skw_sdata0_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sdata0;

	sdata0 = (u8)val;

	if (sdata0 > MAX_SCLK)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_skw_sdata(mipicsi, MIPI_TOP_1, LANE_0, sdata0);
	else
		mipicsi->hw_ops->aphy_set_skw_sdata(mipicsi, MIPI_TOP_0, LANE_0, sdata0);

exit:
	return 0;
}

static int aphy_skw_sdata0_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sdata0;

	if (mipicsi->ch_index >= CH_4)
		sdata0 = mipicsi->hw_ops->aphy_get_skw_sdata(mipicsi, MIPI_TOP_1, LANE_0);
	else
		sdata0 = mipicsi->hw_ops->aphy_get_skw_sdata(mipicsi, MIPI_TOP_0, LANE_0);

	*val = (u64)sdata0;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_skw_sdata0_fops,
		aphy_skw_sdata0_get, aphy_skw_sdata0_set, "%llu\n");

static int aphy_skw_sdata1_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sdata1;

	sdata1 = (u8)val;

	if (sdata1 > MAX_SCLK)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_skw_sdata(mipicsi, MIPI_TOP_1, LANE_1, sdata1);
	else
		mipicsi->hw_ops->aphy_set_skw_sdata(mipicsi, MIPI_TOP_0, LANE_1, sdata1);

exit:
	return 0;
}

static int aphy_skw_sdata1_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sdata1;

	if (mipicsi->ch_index >= CH_4)
		sdata1 = mipicsi->hw_ops->aphy_get_skw_sdata(mipicsi, MIPI_TOP_1, LANE_1);
	else
		sdata1 = mipicsi->hw_ops->aphy_get_skw_sdata(mipicsi, MIPI_TOP_0, LANE_1);

	*val = (u64)sdata1;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_skw_sdata1_fops,
		aphy_skw_sdata1_get, aphy_skw_sdata1_set, "%llu\n");

static int aphy_skw_sdata2_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sdata2;

	sdata2 = (u8)val;

	if (sdata2 > MAX_SCLK)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_skw_sdata(mipicsi, MIPI_TOP_1, LANE_2, sdata2);
	else
		mipicsi->hw_ops->aphy_set_skw_sdata(mipicsi, MIPI_TOP_0, LANE_2, sdata2);

exit:
	return 0;
}

static int aphy_skw_sdata2_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sdata2;

	if (mipicsi->ch_index >= CH_4)
		sdata2 = mipicsi->hw_ops->aphy_get_skw_sdata(mipicsi, MIPI_TOP_1, LANE_2);
	else
		sdata2 = mipicsi->hw_ops->aphy_get_skw_sdata(mipicsi, MIPI_TOP_0, LANE_2);

	*val = (u64)sdata2;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_skw_sdata2_fops,
		aphy_skw_sdata2_get, aphy_skw_sdata2_set, "%llu\n");

static int aphy_skw_sdata3_set(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sdata3;

	sdata3 = (u8)val;

	if (sdata3 > MAX_SCLK)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_skw_sdata(mipicsi, MIPI_TOP_1, LANE_3, sdata3);
	else
		mipicsi->hw_ops->aphy_set_skw_sdata(mipicsi, MIPI_TOP_0, LANE_3, sdata3);

exit:
	return 0;
}

static int aphy_skw_sdata3_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u8 sdata3;

	if (mipicsi->ch_index >= CH_4)
		sdata3 = mipicsi->hw_ops->aphy_get_skw_sdata(mipicsi, MIPI_TOP_1, LANE_3);
	else
		sdata3 = mipicsi->hw_ops->aphy_get_skw_sdata(mipicsi, MIPI_TOP_0, LANE_3);

	*val = (u64)sdata3;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_skw_sdata3_fops,
		aphy_skw_sdata3_get, aphy_skw_sdata3_set, "%llu\n");

static int eq_set_ctune(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u32 ctune;

	ctune = (u32)val;

	if (ctune >= MAX_CTUNE)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_ctune(mipicsi, MIPI_TOP_1, ctune);
	else
		mipicsi->hw_ops->aphy_set_ctune(mipicsi, MIPI_TOP_0, ctune);

exit:
	return 0;
}

static int eq_get_ctune(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u32 ctune;

	if (mipicsi->ch_index >= CH_4)
		ctune = mipicsi->hw_ops->aphy_get_ctune(mipicsi, MIPI_TOP_1);
	else
		ctune = mipicsi->hw_ops->aphy_get_ctune(mipicsi, MIPI_TOP_0);

	*val = (u64)ctune;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_ctune_fops, eq_get_ctune, eq_set_ctune,
			 "%llu\n");

static int eq_set_rtune(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u32 rtune;

	rtune = (u32)val;

	if (rtune >= MAX_RTUNE)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_rtune(mipicsi, MIPI_TOP_1, rtune);
	else
		mipicsi->hw_ops->aphy_set_rtune(mipicsi, MIPI_TOP_0, rtune);

exit:
	return 0;
}

static int eq_get_rtune(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u32 rtune;

	if (mipicsi->ch_index >= CH_4)
		rtune = mipicsi->hw_ops->aphy_get_rtune(mipicsi, MIPI_TOP_1);
	else
		rtune = mipicsi->hw_ops->aphy_get_rtune(mipicsi, MIPI_TOP_0);

	*val = (u64)rtune;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_rtune_fops, eq_get_rtune, eq_set_rtune,
			 "%llu\n");

static int eq_set_d2s(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u32 d2s;

	d2s = (u32)val;

	if (d2s >= MAX_D2S)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_d2s(mipicsi, MIPI_TOP_1, d2s);
	else
		mipicsi->hw_ops->aphy_set_d2s(mipicsi, MIPI_TOP_0, d2s);

exit:
	return 0;
}

static int eq_get_d2s(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u32 d2s;

	if (mipicsi->ch_index >= CH_4)
		d2s = mipicsi->hw_ops->aphy_get_d2s(mipicsi, MIPI_TOP_1);
	else
		d2s = mipicsi->hw_ops->aphy_get_d2s(mipicsi, MIPI_TOP_0);

	*val = (u64)d2s;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_d2s_fops, eq_get_d2s, eq_set_d2s,
			 "%llu\n");

static int eq_set_ibn_dc(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u32 ibn_dc;

	ibn_dc = (u32)val;

	if (ibn_dc >= MAX_IBM_DC)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_ibn_dc(mipicsi, MIPI_TOP_1, ibn_dc);
	else
		mipicsi->hw_ops->aphy_set_ibn_dc(mipicsi, MIPI_TOP_0, ibn_dc);

exit:
	return 0;
}

static int eq_get_ibn_dc(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u32 ibn_dc;

	if (mipicsi->ch_index >= CH_4)
		ibn_dc = mipicsi->hw_ops->aphy_get_ibn_dc(mipicsi, MIPI_TOP_1);
	else
		ibn_dc = mipicsi->hw_ops->aphy_get_ibn_dc(mipicsi, MIPI_TOP_0);

	*val = (u64)ibn_dc;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_ibn_dc_fops, eq_get_ibn_dc, eq_set_ibn_dc,
			 "%llu\n");

static int eq_set_ibn_gm(void *data, u64 val)
{
	struct rtk_mipicsi *mipicsi = data;
	u32 ibn_gm;

	ibn_gm = (u32)val;

	if (ibn_gm >= MAX_IBM_GM)
		goto exit;

	if (mipicsi->ch_index >= CH_4)
		mipicsi->hw_ops->aphy_set_ibn_gm(mipicsi, MIPI_TOP_1, ibn_gm);
	else
		mipicsi->hw_ops->aphy_set_ibn_gm(mipicsi, MIPI_TOP_0, ibn_gm);

exit:
	return 0;
}

static int eq_get_ibn_gm(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;
	u32 ibn_gm;

	if (mipicsi->ch_index >= CH_4)
		ibn_gm = mipicsi->hw_ops->aphy_get_ibn_gm(mipicsi, MIPI_TOP_1);
	else
		ibn_gm = mipicsi->hw_ops->aphy_get_ibn_gm(mipicsi, MIPI_TOP_0);

	*val = (u64)ibn_gm;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_ibn_gm_fops, eq_get_ibn_gm, eq_set_ibn_gm,
			 "%llu\n");

static int aphy_cfg_set(void *data, u64 val)
{

	return 0;
}

static int aphy_cfg_get(void *data, u64 *val)
{
	struct rtk_mipicsi *mipicsi = data;

	if (mipicsi->ch_index >= CH_4) {
		mipicsi->hw_ops->dump_aphy_cfg(mipicsi, MIPI_TOP_1);
		*val = MIPI_TOP_1;
	} else {
		mipicsi->hw_ops->dump_aphy_cfg(mipicsi, MIPI_TOP_0);
		*val = MIPI_TOP_0;
	}

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(mipicsi_dbg_aphy_cfg_fops, aphy_cfg_get, aphy_cfg_set,
			 "%llu\n");

void rtk_mipicsi_setup_dbgfs(struct rtk_mipicsi *mipicsi)
{
	struct rtk_mipicsi_debug *debug = &mipicsi->debug;
	char name[16];

	snprintf(name, 16, "mipicsi_%u", mipicsi->ch_index);
	debug->debugfs_dir = debugfs_create_dir(name, NULL);

	if (IS_ERR_OR_NULL(debug->debugfs_dir)) {
		dev_info(mipicsi->dev, "DebugFS unsupported\n");
		return;
	}

	debugfs_create_bool("en_colorbar", 0644, debug->debugfs_dir, &debug->en_colorbar);

	if ((mipicsi->ch_index == CH_0) || (mipicsi->conf->en_group_dev) ||
		(mipicsi->ch_index == CH_4)) {

		debug->phy_check_ms = DEFAULT_PHY_CHECK_MS;
		debugfs_create_file("phy_check", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_phy_check_fops);

		debug->tuning_sample = DEFAULT_TUNING_SAMPLE;
		debugfs_create_file("eq_tuning", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_eq_tuning_fops);

		debugfs_create_file("en_manual_deskew", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_en_manual_deskew_fops);

		debugfs_create_file("skw_sclk0", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_skw_sclk0_fops);

		debugfs_create_file("skw_sclk1", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_skw_sclk1_fops);

		debugfs_create_file("skw_sclk2", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_skw_sclk2_fops);

		debugfs_create_file("skw_sclk3", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_skw_sclk3_fops);

		debugfs_create_file("skw_sdata0", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_skw_sdata0_fops);

		debugfs_create_file("skw_sdata1", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_skw_sdata1_fops);

		debugfs_create_file("skw_sdata2", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_skw_sdata2_fops);

		debugfs_create_file("skw_sdata3", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_skw_sdata3_fops);

		debugfs_create_file("aphy_ctune", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_ctune_fops);

		debugfs_create_file("aphy_rtune", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_rtune_fops);

		debugfs_create_file("aphy_d2s", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_d2s_fops);

		debugfs_create_file("aphy_ibn_dc", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_ibn_dc_fops);

		debugfs_create_file("aphy_ibn_gm", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_ibn_gm_fops);

		debugfs_create_file("aphy_cfg", 0644, debug->debugfs_dir, mipicsi,
			&mipicsi_dbg_aphy_cfg_fops);
	}
}
