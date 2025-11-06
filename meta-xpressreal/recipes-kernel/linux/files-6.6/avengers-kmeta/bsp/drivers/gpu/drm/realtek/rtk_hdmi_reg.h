/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2019 Realtek Inc.
 */

#ifndef _HDMI_REG_H_INCLUDED_
#define _HDMI_REG_H_INCLUDED_

#define HDMI_GCPCR                                                                   0x078
#define HDMI_GCPCR_enablegcp_shift                                                   (3)
#define HDMI_GCPCR_enablegcp_mask                                                    (0x00000008)
#define HDMI_GCPCR_enablegcp(data)                                                   (0x00000008&((data)<<3))
#define HDMI_GCPCR_get_enablegcp(data)                                               ((0x00000008&(data))>>3)
#define HDMI_GCPCR_gcp_clearavmute_shift                                             (2)
#define HDMI_GCPCR_gcp_clearavmute_mask                                              (0x00000004)
#define HDMI_GCPCR_gcp_clearavmute(data)                                             (0x00000004&((data)<<2))
#define HDMI_GCPCR_get_gcp_clearavmute(data)                                         ((0x00000004&(data))>>2)
#define HDMI_GCPCR_gcp_setavmute_shift                                               (1)
#define HDMI_GCPCR_gcp_setavmute_mask                                                (0x00000002)
#define HDMI_GCPCR_gcp_setavmute(data)                                               (0x00000002&((data)<<1))
#define HDMI_GCPCR_get_gcp_setavmute(data)                                           ((0x00000002&(data))>>1)
#define HDMI_GCPCR_write_data_shift                                                  (0)
#define HDMI_GCPCR_write_data_mask                                                   (0x00000001)
#define HDMI_GCPCR_write_data(data)                                                  (0x00000001&((data)<<0))
#define HDMI_GCPCR_get_write_data(data)                                              ((0x00000001&(data))>>0)

#define HDMI_PHY_STATUS                                                              0x15c
#define HDMI_PHY_STATUS_wdout_shift                                                  (1)
#define HDMI_PHY_STATUS_wdout_mask                                                   (0x00000002)
#define HDMI_PHY_STATUS_wdout(data)                                                  (0x00000002&((data)<<1))
#define HDMI_PHY_STATUS_wdout_src(data)                                              ((0x00000002&(data))>>1)
#define HDMI_PHY_STATUS_get_wdout(data)                                              ((0x00000002&(data))>>1)
#define HDMI_PHY_STATUS_Rxstatus_shift                                               (0)
#define HDMI_PHY_STATUS_Rxstatus_mask                                                (0x00000001)
#define HDMI_PHY_STATUS_Rxstatus(data)                                               (0x00000001&((data)<<0))
#define HDMI_PHY_STATUS_Rxstatus_src(data)                                           ((0x00000001&(data))>>0)
#define HDMI_PHY_STATUS_get_Rxstatus(data)                                           ((0x00000001&(data))>>0)

/* HDMI_TOP */

#define PLLDIV0                                                                      0x10
#define PLLDIV0_reg_addr                                                             "0x9804d810"
#define PLLDIV0_reg                                                                  0x9804d810
#define PLLDIV0_hdmitx_plltmds_clk_count_shift                                       (13)
#define PLLDIV0_hdmitx_plltmds_clk_count_mask                                        (0x3fffe000)
#define PLLDIV0_hdmitx_plltmds_clk_count(data)                                       (0x3fffe000&((data)<<13))
#define PLLDIV0_hdmitx_plltmds_clk_count_src(data)                                   ((0x3fffe000&(data))>>13)
#define PLLDIV0_get_hdmitx_plltmds_clk_count(data)                                   ((0x3fffe000&(data))>>13)
#define PLLDIV0_hdmitx_plltmds_refclk_count_shift                                    (2)
#define PLLDIV0_hdmitx_plltmds_refclk_count_mask                                     (0x1ffc)
#define PLLDIV0_hdmitx_plltmds_refclk_count(data)                                    (0x1ffc&((data)<<2))
#define PLLDIV0_hdmitx_plltmds_refclk_count_src(data)                                ((0x1ffc&(data))>>2)
#define PLLDIV0_get_hdmitx_plltmds_refclk_count(data)                                ((0x1ffc&(data))>>2)
#define PLLDIV0_hdmitx_plltmds_count_en_shift                                        (1)
#define PLLDIV0_hdmitx_plltmds_count_en_mask                                         (0x2)
#define PLLDIV0_hdmitx_plltmds_count_en(data)                                        (0x2&((data)<<1))
#define PLLDIV0_hdmitx_plltmds_count_en_src(data)                                    ((0x2&(data))>>1)
#define PLLDIV0_get_hdmitx_plltmds_count_en(data)                                    ((0x2&(data))>>1)
#define PLLDIV0_hdmitx_plltmds_rstn_shift                                            (0)
#define PLLDIV0_hdmitx_plltmds_rstn_mask                                             (0x1)
#define PLLDIV0_hdmitx_plltmds_rstn(data)                                            (0x1&((data)<<0))
#define PLLDIV0_hdmitx_plltmds_rstn_src(data)                                        ((0x1&(data))>>0)
#define PLLDIV0_get_hdmitx_plltmds_rstn(data)                                        ((0x1&(data))>>0)

#define PLLDIV0_SETUP                                                                0x14
#define PLLDIV0_SETUP_reg_addr                                                       "0x9804d814"
#define PLLDIV0_SETUP_reg                                                            0x9804d814
#define PLLDIV0_SETUP_hdmitx_plltmds_upper_bound_shift                               (16)
#define PLLDIV0_SETUP_hdmitx_plltmds_upper_bound_mask                                (0x1fff0000)
#define PLLDIV0_SETUP_hdmitx_plltmds_upper_bound(data)                               (0x1fff0000&((data)<<16))
#define PLLDIV0_SETUP_hdmitx_plltmds_upper_bound_src(data)                           ((0x1fff0000&(data))>>16)
#define PLLDIV0_SETUP_get_hdmitx_plltmds_upper_bound(data)                           ((0x1fff0000&(data))>>16)
#define PLLDIV0_SETUP_hdmitx_plltmds_lower_bound_shift                               (0)
#define PLLDIV0_SETUP_hdmitx_plltmds_lower_bound_mask                                (0x1fff)
#define PLLDIV0_SETUP_hdmitx_plltmds_lower_bound(data)                               (0x1fff&((data)<<0))
#define PLLDIV0_SETUP_hdmitx_plltmds_lower_bound_src(data)                           ((0x1fff&(data))>>0)
#define PLLDIV0_SETUP_get_hdmitx_plltmds_lower_bound(data)                           ((0x1fff&(data))>>0)

#define PLLDIV_SEL                                                                   0x3c
#define PLLDIV_SEL_reg_addr                                                          "0x9804d83c"
#define PLLDIV_SEL_reg                                                               0x9804d83c
#define PLLDIV_SEL_tmds_src_sel_shift                                                (0)
#define PLLDIV_SEL_tmds_src_sel_mask                                                 (0x1)
#define PLLDIV_SEL_tmds_src_sel(data)                                                (0x1&((data)<<0))
#define PLLDIV_SEL_tmds_src_sel_src(data)                                            ((0x1&(data))>>0)
#define PLLDIV_SEL_get_tmds_src_sel(data)                                            ((0x1&(data))>>0)

#define RXST                                                                         0x40
#define RXST_reg_addr                                                                "0x9804d840"
#define RXST_reg                                                                     0x9804d840
#define RXST_rxsenseint_shift                                                        (2)
#define RXST_rxsenseint_mask                                                         (0x4)
#define RXST_rxsenseint(data)                                                        (0x4&((data)<<2))
#define RXST_rxsenseint_src(data)                                                    ((0x4&(data))>>2)
#define RXST_get_rxsenseint(data)                                                    ((0x4&(data))>>2)
#define RXST_Rxstatus_shift                                                          (1)
#define RXST_Rxstatus_mask                                                           (0x2)
#define RXST_Rxstatus(data)                                                          (0x2&((data)<<1))
#define RXST_Rxstatus_src(data)                                                      ((0x2&(data))>>1)
#define RXST_get_Rxstatus(data)                                                      ((0x2&(data))>>1)
#define RXST_rxupdated_shift                                                         (0)
#define RXST_rxupdated_mask                                                          (0x1)
#define RXST_rxupdated(data)                                                         (0x1&((data)<<0))
#define RXST_rxupdated_src(data)                                                     ((0x1&(data))>>0)
#define RXST_get_rxupdated(data)                                                     ((0x1&(data))>>0)

#define PLLDIV0_MP_STATUS                                                            0x70
#define PLLDIV0_MP_STATUS_reg_addr                                                   "0x9804d870"
#define PLLDIV0_MP_STATUS_reg                                                        0x9804d870
#define PLLDIV0_MP_STATUS_hdmitx_plltmds_clkdet_cmp_result_shift                     (1)
#define PLLDIV0_MP_STATUS_hdmitx_plltmds_clkdet_cmp_result_mask                      (0x2)
#define PLLDIV0_MP_STATUS_hdmitx_plltmds_clkdet_cmp_result(data)                     (0x2&((data)<<1))
#define PLLDIV0_MP_STATUS_hdmitx_plltmds_clkdet_cmp_result_src(data)                 ((0x2&(data))>>1)
#define PLLDIV0_MP_STATUS_get_hdmitx_plltmds_clkdet_cmp_result(data)                 ((0x2&(data))>>1)
#define PLLDIV0_MP_STATUS_hdmitx_plltmds_clkdet_done_shift                           (0)
#define PLLDIV0_MP_STATUS_hdmitx_plltmds_clkdet_done_mask                            (0x1)
#define PLLDIV0_MP_STATUS_hdmitx_plltmds_clkdet_done(data)                           (0x1&((data)<<0))
#define PLLDIV0_MP_STATUS_hdmitx_plltmds_clkdet_done_src(data)                       ((0x1&(data))>>0)
#define PLLDIV0_MP_STATUS_get_hdmitx_plltmds_clkdet_done(data)                       ((0x1&(data))>>0)

#endif /* _HDMI_REG_H_INCLUDED_ */
