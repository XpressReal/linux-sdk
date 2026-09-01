#ifndef __SYSDEPS_RTK_OPP_SYSDEPS_H
#define __SYSDEPS_RTK_OPP_SYSDEPS_H

#include <common.h>

#define rtk_opp_printf printf
#define rtk_opp_snprintf snprintf

#ifdef DEBUG
#define rtk_opp_dbg_print rtk_opp_printf
#else
#define rtk_opp_dbg_print(...)  (0)
#endif

#endif /* __SYSDEPS_RTK_OPP_SYSDEPS_H */
