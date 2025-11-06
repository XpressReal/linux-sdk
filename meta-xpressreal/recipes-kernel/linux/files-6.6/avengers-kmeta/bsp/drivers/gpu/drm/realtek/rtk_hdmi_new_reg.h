/*
 * hdmi_new_reg.h - RTK hdmitx driver header file
 * Version 2021-8-18
 */
#ifndef _HDMI_NEW_REG_H_INCLUDED_
#define _HDMI_NEW_REG_H_INCLUDED_

#define HDMI_NEW_TGEN0                                                               0x200
#define HDMI_NEW_TGEN0_reg_addr                                                      "0x98121200"
#define HDMI_NEW_TGEN0_reg                                                           0x98121200

#define HDMI_NEW_TGEN1                                                               0x204
#define HDMI_NEW_TGEN1_reg_addr                                                      "0x98121204"
#define HDMI_NEW_TGEN1_reg                                                           0x98121204

#define HDMI_NEW_TGEN3                                                               0x20c
#define HDMI_NEW_TGEN3_reg_addr                                                      "0x9812120c"
#define HDMI_NEW_TGEN3_reg                                                           0x9812120c

#define HDMI_NEW_TGEN4                                                               0x210
#define HDMI_NEW_TGEN4_reg_addr                                                      "0x98121210"
#define HDMI_NEW_TGEN4_reg                                                           0x98121210

#define HDMI_NEW_TGEN5                                                               0x214
#define HDMI_NEW_TGEN5_reg_addr                                                      "0x98121214"
#define HDMI_NEW_TGEN5_reg                                                           0x98121214

#define HDMI_NEW_TGEN6                                                               0x218
#define HDMI_NEW_TGEN6_reg_addr                                                      "0x98121218"
#define HDMI_NEW_TGEN6_reg                                                           0x98121218

#define HDMI_NEW_TGEN7                                                               0x21c
#define HDMI_NEW_TGEN7_reg_addr                                                      "0x9812121c"
#define HDMI_NEW_TGEN7_reg                                                           0x9812121c

#define HDMI_NEW_TGEN8                                                               0x220
#define HDMI_NEW_TGEN8_reg_addr                                                      "0x98121220"
#define HDMI_NEW_TGEN8_reg                                                           0x98121220

#define HDMI_NEW_PKT_GCP0                                                            0x620
#define HDMI_NEW_PKT_GCP0_reg_addr                                                   "0x98121620"
#define HDMI_NEW_PKT_GCP0_reg                                                        0x98121620
#define HDMI_NEW_PKT_GCP0_gcp_en_shift                                               (28)
#define HDMI_NEW_PKT_GCP0_gcp_en_mask                                                (0x10000000)
#define HDMI_NEW_PKT_GCP0_gcp_en(data)                                               (0x10000000&((data)<<28))
#define HDMI_NEW_PKT_GCP0_gcp_en_src(data)                                           ((0x10000000&(data))>>28)
#define HDMI_NEW_PKT_GCP0_get_gcp_en(data)                                           ((0x10000000&(data))>>28)
#define HDMI_NEW_PKT_GCP0_set_avmute_shift                                           (17)
#define HDMI_NEW_PKT_GCP0_set_avmute_mask                                            (0x20000)
#define HDMI_NEW_PKT_GCP0_set_avmute(data)                                           (0x20000&((data)<<17))
#define HDMI_NEW_PKT_GCP0_set_avmute_src(data)                                       ((0x20000&(data))>>17)
#define HDMI_NEW_PKT_GCP0_get_set_avmute(data)                                       ((0x20000&(data))>>17)
#define HDMI_NEW_PKT_GCP0_clr_avmute_shift                                           (16)
#define HDMI_NEW_PKT_GCP0_clr_avmute_mask                                            (0x10000)
#define HDMI_NEW_PKT_GCP0_clr_avmute(data)                                           (0x10000&((data)<<16))
#define HDMI_NEW_PKT_GCP0_clr_avmute_src(data)                                       ((0x10000&(data))>>16)
#define HDMI_NEW_PKT_GCP0_get_clr_avmute(data)                                       ((0x10000&(data))>>16)
#define HDMI_NEW_PKT_GCP0_force_gcp_cd_shift                                         (12)
#define HDMI_NEW_PKT_GCP0_force_gcp_cd_mask                                          (0x1000)
#define HDMI_NEW_PKT_GCP0_force_gcp_cd(data)                                         (0x1000&((data)<<12))
#define HDMI_NEW_PKT_GCP0_force_gcp_cd_src(data)                                     ((0x1000&(data))>>12)
#define HDMI_NEW_PKT_GCP0_get_force_gcp_cd(data)                                     ((0x1000&(data))>>12)
#define HDMI_NEW_PKT_GCP0_gcp_cd_sw_shift                                            (8)
#define HDMI_NEW_PKT_GCP0_gcp_cd_sw_mask                                             (0xf00)
#define HDMI_NEW_PKT_GCP0_gcp_cd_sw(data)                                            (0xf00&((data)<<8))
#define HDMI_NEW_PKT_GCP0_gcp_cd_sw_src(data)                                        ((0xf00&(data))>>8)
#define HDMI_NEW_PKT_GCP0_get_gcp_cd_sw(data)                                        ((0xf00&(data))>>8)
#define HDMI_NEW_PKT_GCP0_force_gcp_pp_shift                                         (4)
#define HDMI_NEW_PKT_GCP0_force_gcp_pp_mask                                          (0x10)
#define HDMI_NEW_PKT_GCP0_force_gcp_pp(data)                                         (0x10&((data)<<4))
#define HDMI_NEW_PKT_GCP0_force_gcp_pp_src(data)                                     ((0x10&(data))>>4)
#define HDMI_NEW_PKT_GCP0_get_force_gcp_pp(data)                                     ((0x10&(data))>>4)
#define HDMI_NEW_PKT_GCP0_gcp_pp_sw_shift                                            (0)
#define HDMI_NEW_PKT_GCP0_gcp_pp_sw_mask                                             (0xf)
#define HDMI_NEW_PKT_GCP0_gcp_pp_sw(data)                                            (0xf&((data)<<0))
#define HDMI_NEW_PKT_GCP0_gcp_pp_sw_src(data)                                        ((0xf&(data))>>0)
#define HDMI_NEW_PKT_GCP0_get_gcp_pp_sw(data)                                        ((0xf&(data))>>0)

#define HDMI_NEW_CHSWAP_CTRL                                                         0xc00
#define HDMI_NEW_CHSWAP_CTRL_reg_addr                                                "0x98121c00"
#define HDMI_NEW_CHSWAP_CTRL_reg                                                     0x98121c00

#endif
