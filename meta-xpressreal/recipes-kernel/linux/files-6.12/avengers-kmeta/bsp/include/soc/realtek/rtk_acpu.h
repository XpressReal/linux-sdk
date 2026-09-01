#ifndef __SOC_REALTEK_ACPU_H
#define __SOC_REALTEK_ACPU_H

#ifdef CONFIG_RTK_ACPU_VE1
int rtk_acpu_release_ve1(void);
#else

static inline int rtk_acpu_release_ve1(void)
{
	return -EINVAL;
}

#endif

#endif
