#include <asm/arch/io.h>

#define ISO_TCW1CR            0x98007a20
#define ISO_TCW1TR            0x98007a24
#define ISO_TCW1NMI           0x98007a28
#define ISO_TCW1OV            0x98007a2c
#define ISO_TCW1OV_RSTB_CNT   0x98007a30
#define ISO_TCW1OV_RSTB_PAD   0x98007a34
#define ISO_TCW1ST            0x98007a38

void watchdog_kick(void)
{
	rtd_outl(ISO_TCW1TR, 1);
}

void watchdog_disable(void)
{
	rtd_maskl(ISO_TCW1CR, ~0xff, 0xa5);
}

static unsigned int watchdog_timeout_to_ov(unsigned int timeout)
{
	unsigned int ov = 0xffffffff;

        if (timeout < 160)
		ov = timeout * 27000000;
	return ov;
}

void watchdog_enable(unsigned int timeout)
{
	rtd_outl(ISO_TCW1OV, watchdog_timeout_to_ov(timeout));
	rtd_outl(ISO_TCW1NMI, 0x07ff0000);
	rtd_maskl(ISO_TCW1CR, ~0xff, 0x00);
}
