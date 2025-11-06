#define PMUX_BASE_ISO	0x9804E000
#define PMUX_BASE_ISOM  0x98146200
#define PMUX_BASE_VE4  0x9814e000
#define PMUX_BASE_MAIN2  0x9804F200

#define PADDRI_4_8 1
#define PADDRI_2_4 0
#define PADDRI_UNSUPPORT 0xFFFF

#define PCOF_UNSUPPORT 0xFFFF
#define PMUX_UNSUPPORT 0xFFFF

#define PULL_DISABLE	0
#define PULL_DOWN	1
#define PULL_UP		2

#define GPIO_TOTAL   166

struct rtk_pin_regmap{
	unsigned int pmux_base;
	unsigned int pmux_regoff;
	unsigned int pmux_regbit;
	unsigned int pmux_regbitmsk;
	unsigned int pcof_regoff;
	unsigned int pcof_regbit;
	unsigned int pcof_cur_strgh;//0 : 2&4mA   1:4&8mA
};

int getISOGPIO(int ISOGPIO_NUM);
int setISOGPIO(int ISOGPIO_NUM, int value);
int setISOGPIO_pullsel(int ISOGPIO_NUM, unsigned char pull_sel);
int getGPIO(int GPIO_NUM);
int setGPIO(int GPIO_NUM, int value);
int setGPIO_pullsel(unsigned int GPIO_NUM, unsigned char pull_sel);

static const struct rtk_pin_regmap pin_regmap[] = {
	{.pmux_base = PMUX_BASE_ISOM, .pmux_regoff = 0x0, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x4, .pcof_regbit = 6, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_0 */
	{.pmux_base = PMUX_BASE_ISOM, .pmux_regoff = 0x0, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x4, .pcof_regbit = 12, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_1 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x0, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x1C, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_2 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x0, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x1C, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_3 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x0, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x1C, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_4 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x0, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x1C, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_5 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x0, .pmux_regbit = 16, .pmux_regbitmsk = 0x1F, .pcof_regoff = 0x1C, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_6 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x0, .pmux_regbit = 21, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_7 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x0, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_8 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x0, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_9 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x0, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_10 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x0, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_11 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x0, .pmux_regbit = 25, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_12 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x4, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_13 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x4, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 27, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_14 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x4, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 30, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_15 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x4, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_16 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x4, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_17 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x4, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x24, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_18 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x4, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x24, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_19 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x4, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 30, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_15 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x4, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x2C, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_20 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x4, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x2C, .pcof_regbit = 10, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_21 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x4, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x24, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_23 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x4, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x24, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_24 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x4, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x24, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_25 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x0, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x50, .pcof_regbit = PCOF_UNSUPPORT, .pcof_cur_strgh = PADDRI_4_8}, /* usb_cc1 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x0, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x50, .pcof_regbit = PCOF_UNSUPPORT, .pcof_cur_strgh = PADDRI_4_8}, /* usb_cc2 */
	{.pmux_base = PMUX_BASE_ISOM, .pmux_regoff = 0x0, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x4, .pcof_regbit = 18, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_28 */
	{.pmux_base = PMUX_BASE_ISOM, .pmux_regoff = 0x0, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x4, .pcof_regbit = 24, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_29 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x8, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_30 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x8, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_31 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x8, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_32 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x8, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_33 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x8, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_34 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x8, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x2C, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_35 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x8, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x48, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* hif_data */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x8, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x48, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* hif_en */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x8, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x4C, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* hif_rdy */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x8, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x44, .pcof_regbit = 10, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* hif_clk */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x8, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_40 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x8, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_41 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x8, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x2C, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_42 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x8, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x2C, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_43 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0xC, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x2C, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_44 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x0, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x24, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_45 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x0, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x24, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_46 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x4, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_47 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x4, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_48 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x4, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x2C, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_49 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x4, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x2C, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_50 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0xC, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x2C, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_51 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x4, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x2C, .pcof_regbit = 27, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_52 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0xC, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_53 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0xC, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_54 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0xC, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_55 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0xC, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_56 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0xC, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_57 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0xC, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_58 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x10, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_59 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x10, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_60 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x10, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_61 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x10, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_62 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x10, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_63 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x8, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_64 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0xC, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_65 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0xC, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_66 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0xC, .pmux_regbit = 8, .pmux_regbitmsk = 0x3F, .pcof_regoff = 0x38, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* gpio_67 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x0, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x18, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_data_0 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x0, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x18, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_data_1 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x0, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x1C, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_data_2 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x0, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x1C, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_data_3 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x4, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_data_4 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x4, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x20, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_data_5 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x4, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x24, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_data_6 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x4, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x24, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_data_7 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x0, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_rst_n */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x0, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x14, .pcof_regbit = 13, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_cmd */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x0, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x14, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_clk */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x0, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x28, .pcof_regbit = 0, .pcof_cur_strgh = PCOF_UNSUPPORT}, /* emmc_dd_sb */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0xC, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 27, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_80 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0xC, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 30, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_81 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0xC, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_82 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0xC, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 4, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_83 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0xC, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_84 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x10, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 10, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_85 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x10, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_86 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x10, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 23, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_87 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x10, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_88 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x10, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 11, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_89 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x10, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 21, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_90 */
	{.pmux_base = PMUX_BASE_MAIN2, .pmux_regoff = 0x10, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x44, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_91 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x10, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_92 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x10, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_93 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x4, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_94 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x4, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_95 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x4, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_96 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x8, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_97 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x8, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x30, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_98 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x8, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_99 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x8, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_100 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x8, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_101 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x8, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_102 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x8, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x34, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_103 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x8, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_104 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0xC, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_105 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0xC, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_106 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0xC, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_107 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0xC, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_108 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0xC, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_109 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0xC, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_110 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0xC, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_111 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x10, .pmux_regbit = 0, .pmux_regbitmsk = 0x1F, .pcof_regoff = 0x3C, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_112 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x10, .pmux_regbit = 5, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_128 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x10, .pmux_regbit = 9, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_129 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x10, .pmux_regbit = 13, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_130 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x10, .pmux_regbit = 17, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_131 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x10, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_132 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x14, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x38, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_133 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x14, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_134 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x14, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_135 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x14, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_136 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x14, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_137 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x14, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 22, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_138 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x14, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_139 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x14, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x3C, .pcof_regbit = 28, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_140 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x18, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_141 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x18, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 4, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_142 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x18, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_143 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x18, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 10, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_144 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x10, .pmux_regbit = 21, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_145 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x10, .pmux_regbit = 25, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_146 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x14, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x44, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_147 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x14, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x44, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_148 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x14, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x44, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_149 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x14, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x44, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_150 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x14, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x44, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_151 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x14, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x48, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_152 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x14, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x48, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_153 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x14, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x48, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_154 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x18, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x48, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_155 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x18, .pmux_regbit = 4, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x48, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_156 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x18, .pmux_regbit = 8, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x4C, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_157 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x18, .pmux_regbit = 12, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x4C, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_158 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x18, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x4C, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_159 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x18, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x4C, .pcof_regbit = 19, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_160 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x18, .pmux_regbit = 24, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x4C, .pcof_regbit = 25, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_161 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x18, .pmux_regbit = 28, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x50, .pcof_regbit = 1, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_162 */
	{.pmux_base = PMUX_BASE_ISO, .pmux_regoff = 0x1C, .pmux_regbit = 0, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x50, .pcof_regbit = 7, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_163 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x18, .pmux_regbit = 16, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 13, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_164 */
	{.pmux_base = PMUX_BASE_VE4, .pmux_regoff = 0x18, .pmux_regbit = 20, .pmux_regbitmsk = 0xF, .pcof_regoff = 0x40, .pcof_regbit = 16, .pcof_cur_strgh = PADDRI_4_8}, /* gpio_165 */
};

