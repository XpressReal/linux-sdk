
#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <generic-phy.h>
#include <i2c.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <pci.h>
#include <regmap.h>
#include <syscon.h>
#include <asm-generic/gpio.h>

#include "pcie_dw_common.h"
#include "pcie-rtd.h"
#include "pcie-rtd-clk-rst.h"

/**
 * pci_address_conversion() - Convert BDF to PCIe address format
 */
static u32 pci_address_conversion(pci_dev_t bdf, uint offset)
{
	int busno = PCI_BUS(bdf);
	int dev = PCI_DEV(bdf);
	int func = PCI_FUNC(bdf);

	return (busno << 24) | (dev << 19) | (func << 16) | (offset & 0xFFFF);
}

/**
 * pci_byte_mask() - Calculate byte mask for config access
 */
static u8 pci_byte_mask(u32 addr, enum pci_size_t size)
{
	int shift = addr & 3;

	if (size == PCI_SIZE_8)
		return 1 << shift;
	else if (size == PCI_SIZE_16)
		return 3 << shift;
	else if (size == PCI_SIZE_32)
		return 0xF;

	return 0;
}

/**
 * indirect_cfg_read() - Perform indirect configuration read
 */
static int indirect_cfg_read(struct rtd_pcie_port *pp, pci_dev_t bdf, u32 addr,
			     u32 *pdata, enum pci_size_t size)
{
	int busno = PCI_BUS(bdf);
	u32 status, rcpl_st;
	u8 mask;
	int try_count = 1000;
	int shift = (addr & 3) * 8;

	/* Check for valid device on root bus */
	if (busno == pp->first_busno && PCI_DEV(bdf) != 0)
		return -EINVAL;

	mask = pci_byte_mask(addr, size);
	if (!mask)
		return -EINVAL;

	/* Only debug first device on bus 1 to reduce spam */
	bool verbose = (busno == 1 && PCI_DEV(bdf) == 0);

	/* Set indirect control - 0x10 for root bus, 0x14 for downstream */
	if (busno == pp->first_busno)
		writel(0x10, pp->ctrl_base + PCIE_INDIR_CTR);
	else
		writel(0x14, pp->ctrl_base + PCIE_INDIR_CTR);

	/* Clear status */
	writel(CFG_ST_ERROR | CFG_ST_DONE, pp->ctrl_base + PCIE_CFG_ST);

	/* Set address */
	writel(addr & ~0x3, pp->ctrl_base + PCIE_CFG_ADDR);

	/* Set byte enable and trigger read */
	writel(BYTE_CNT(mask) | BYTE_EN | WRRD_EN(0), pp->ctrl_base + PCIE_CFG_EN);

	if (verbose) {
		debug("  INDIR_CTR=0x%x, CFG_ADDR=0x%x, CFG_EN=0x%x\n",
		      readl(pp->ctrl_base + PCIE_INDIR_CTR),
		      readl(pp->ctrl_base + PCIE_CFG_ADDR),
		      readl(pp->ctrl_base + PCIE_CFG_EN));
	}

	writel(GO_CT, pp->ctrl_base + PCIE_CFG_CT);

	/* Wait for completion */
	do {
		status = readl(pp->ctrl_base + PCIE_CFG_ST);
		udelay(50);
	} while (!(status & CFG_ST_DONE) && try_count--);

	if (try_count < 0) {
		debug("PCIe: Config read timeout (addr=0x%x)\n", addr);
		goto error;
	}

	/* Check completion status */
	rcpl_st = readl(pp->ctrl_base + PCIE_RCPL_ST);

	if (verbose) {
		debug("  CFG_ST=0x%x, RCPL_ST=0x%x, tries_left=%d\n",
		      status, rcpl_st, try_count);
	}

	if (status & CFG_ST_ERROR) {
		debug("PCIe: Config read error (addr=0x%x, CFG_ST=0x%x)\n", addr, status);
		goto error;
	}

	/* Check for completion errors (Unsupported Request, etc.) */
	if ((rcpl_st & 0xE0) != 0) {
		u8 err_st = (rcpl_st >> 5) & 0x7;
		if (verbose) {
			debug("PCIe: Completion error: 0x%x (UR/CRS/CA)\n", err_st);
		}
		/* This is expected when no device present - return 0xFFFFFFFF */
		*pdata = 0xFFFFFFFF;
		writel(CFG_ST_ERROR | CFG_ST_DONE, pp->ctrl_base + PCIE_CFG_ST);
		return 0;
	}

	/* Clear status */
	writel(CFG_ST_ERROR | CFG_ST_DONE, pp->ctrl_base + PCIE_CFG_ST);

	/* Read data and apply mask */
	*pdata = readl(pp->ctrl_base + PCIE_CFG_RDATA);

	if (verbose) {
		debug("  CFG_RDATA=0x%x (before shift/mask)\n", *pdata);
	}

	*pdata = (*pdata >> shift) & pci_get_ff(size);

	return 0;

error:
	writel(CFG_ST_ERROR | CFG_ST_DONE, pp->ctrl_base + PCIE_CFG_ST);
	return -EIO;
}

/**
 * rtd_pcie_read_config() - Read from PCI configuration space
 */
static int rtd_pcie_read_config(const struct udevice *bus, pci_dev_t bdf,
				uint offset, ulong *valuep,
				enum pci_size_t size)
{
	struct rtd_pcie_port *pp = dev_get_priv(bus);
	int busno = PCI_BUS(bdf);
	int devno = PCI_DEV(bdf);
	int funcno = PCI_FUNC(bdf);
	u32 address;
	u32 value;
	int ret;

	/* Only bus 0, device 0, function 0 exists */
	if (busno != pp->first_busno || devno != 0 || funcno != 0) {
		*valuep = pci_get_ff(size);
		return 0;
	}

	/*
	 * Realtek PCIe controller quirk:
	 * - The endpoint device appears at bus 0, device 0 (not bus 1)
	 * - Direct read = root complex config (vendor 0x20ec, device 0x6698)
	 * - Indirect read with INDIR_CTR=0x10 = actual endpoint device
	 */
	address = pci_address_conversion(bdf, offset);
	ret = indirect_cfg_read(pp, bdf, address, &value, size);
	if (ret) {
		*valuep = pci_get_ff(size);
		return 0;
	}

	*valuep = value;
	return 0;
}

/**
 * indirect_cfg_write() - Perform indirect configuration write
 */
static int indirect_cfg_write(struct rtd_pcie_port *pp, pci_dev_t bdf, u32 addr,
			      u32 data, enum pci_size_t size)
{
	int busno = PCI_BUS(bdf);
	u32 status;
	u8 mask;
	int try_count = 1000;
	int shift = (addr & 3) * 8;

	/* Check for valid device on root bus */
	if (busno == pp->first_busno && PCI_DEV(bdf) != 0)
		return -EINVAL;

	mask = pci_byte_mask(addr, size);
	if (!mask)
		return -EINVAL;

	/* Prepare data with proper shift */
	data = (data & pci_get_ff(size)) << shift;

	/* Set indirect control - 0x12 for root bus, 0x16 for downstream */
	if (busno == pp->first_busno)
		writel(0x12, pp->ctrl_base + PCIE_INDIR_CTR);
	else
		writel(0x16, pp->ctrl_base + PCIE_INDIR_CTR);

	/* Clear status */
	writel(CFG_ST_ERROR | CFG_ST_DONE, pp->ctrl_base + PCIE_CFG_ST);

	/* Set address and data */
	writel(addr & ~0x3, pp->ctrl_base + PCIE_CFG_ADDR);
	writel(data, pp->ctrl_base + PCIE_CFG_WDATA);

	/* Set byte enable and trigger write */
	if (size == PCI_SIZE_32)
		writel(0x1, pp->ctrl_base + PCIE_CFG_EN);
	else
		writel(BYTE_CNT(mask) | BYTE_EN | WRRD_EN(1), pp->ctrl_base + PCIE_CFG_EN);

	writel(GO_CT, pp->ctrl_base + PCIE_CFG_CT);

	/* Wait for completion */
	do {
		status = readl(pp->ctrl_base + PCIE_CFG_ST);
		udelay(50);
	} while (!(status & CFG_ST_DONE) && try_count--);

	if (try_count < 0) {
		debug("PCIe: Config write timeout (addr=0x%x)\n", addr);
		goto error;
	}

	if (status & CFG_ST_ERROR) {
		debug("PCIe: Config write error (addr=0x%x)\n", addr);
		goto error;
	}

	/* Clear status */
	writel(CFG_ST_ERROR | CFG_ST_DONE, pp->ctrl_base + PCIE_CFG_ST);

	return 0;

error:
	writel(CFG_ST_ERROR | CFG_ST_DONE, pp->ctrl_base + PCIE_CFG_ST);
	return -EIO;
}

/**
 * rtd_pcie_write_config() - Write to PCI configuration space
 */
static int rtd_pcie_write_config(struct udevice *bus, pci_dev_t bdf,
				 uint offset, ulong value,
				 enum pci_size_t size)
{
	struct rtd_pcie_port *pp = dev_get_priv(bus);
	int busno = PCI_BUS(bdf);
	int devno = PCI_DEV(bdf);
	int funcno = PCI_FUNC(bdf);
	u32 address;

	/* Only bus 0, device 0, function 0 exists */
	if (busno != pp->first_busno || devno != 0 || funcno != 0)
		return 0;

	/* Use indirect config to write to endpoint device */
	address = pci_address_conversion(bdf, offset);
	indirect_cfg_write(pp, bdf, address, value, size);

	/* Ensure config writes are visible before returning */
	mb();

	return 0;
}



static int check_pipe_clock_ready(struct rtd_pcie_port *pp)
{
	int timeout = 50;
	int status;
	int ret = 0;

	status = readl(pp->ctrl_base + PCIE_MAC_ST) & BIT(16);
	while (!(readl(pp->ctrl_base + PCIE_MAC_ST) & BIT(16)) && --timeout) {
		udelay(10);
	}

	if (!timeout) {
		ret = -EBUSY;
		dev_err(pp->dev, "pipe clock timeout\n");
	}

	return ret;
}

static u32 get_pcie_mac_stat(struct rtd_pcie_port *pp)
{
	int timeout = 10000;
	u32 mac_stat, mac_stat_tmp;

	mac_stat_tmp = readl(pp->ctrl_base + PCIE_MAC_ST);
	while (timeout) {
		mac_stat = readl(pp->ctrl_base + PCIE_MAC_ST);
		if (mac_stat == mac_stat_tmp)
			return mac_stat;
		else
			mac_stat_tmp = mac_stat;
		timeout--;
	}

	return 0;
}

static int pcie_link_init(struct rtd_pcie_port *pp)
{
	int ret = 0;
	int timeout;
	u32 tmp;
	int cur_link_speed;
	u32 ltssm;
	u32 mac_stat;

	dev_dbg(pp->dev, "Link initialization start\n");

	/* Assert PERST# */
	ret = dm_gpio_set_value(&pp->perst_gpio, 0);
	if (ret) {
		dev_err(pp->dev, "Failed to assert PERST#\n");
		return ret;
	}

	writel(0x00140010, pp->ctrl_base + PCIE_SYS_CTR);
	if (pp->speed_mode != 0) {
		tmp = readl(pp->ctrl_base + LINK_CONTROL2_LINK_STATUS2_REG);
		writel((tmp & ~0xf) | pp->speed_mode, pp->ctrl_base + LINK_CONTROL2_LINK_STATUS2_REG);
		dev_info(pp->dev, "Host Set Link Speed on Gen%d\n", pp->speed_mode);
	}

	ret = generic_phy_init(&pp->pcie_phy);
	if (ret) {
		dev_err(pp->dev, "Failed to initialize PHY: %d\n", ret);
		return ret;
	}

	if (pp->ops->init2)
		pp->ops->init2(pp);

	mdelay(100);

	/* Deassert PERST# */
	ret = dm_gpio_set_value(&pp->perst_gpio, 1);
	if (ret) {
		dev_err(pp->dev, "Failed to deassert PERST#\n");
		return ret;
	}

	/* Enable link training */
	writel(0x00010120, pp->ctrl_base + PORT_LINK_CTRL_OFF);
	writel(0x001E0022, pp->ctrl_base + PCIE_SYS_CTR);

	/* Wait for link up */
	dev_dbg(pp->dev, "Waiting for link up...\n");

	timeout = PCIE_CONNECT_TIMEOUT;
	while (timeout > 0) {
		mac_stat = get_pcie_mac_stat(pp);
		if (mac_stat & BIT(11)) {
			ltssm = (mac_stat & GENMASK(9, 4)) >> 4;
			if (ltssm == 0x11) {
				mdelay(1);
				mac_stat = get_pcie_mac_stat(pp);
				ltssm = (mac_stat & GENMASK(9, 4)) >> 4;
				if (ltssm == 0x11)
					break;
			}
		}
		udelay(50);
		timeout--;
	}
	if (timeout == 0) {
		dev_err(pp->dev, "Link down - no device connected, mac_stat=0x%x, ltssm=0x%x\n", mac_stat, ltssm);
		return -ENODEV;
	}

	dev_info(pp->dev, "Link up!\n");

	cur_link_speed = (readl(pp->ctrl_base + LINK_CONTROL_LINK_STATUS_REG) & GENMASK(19, 16)) >> 16;


	switch (cur_link_speed) {
	case 0x1:
		dev_info(pp->dev, "Current Link Speed: 2.5GT/s\n");
		break;
	case 0x2:
		dev_info(pp->dev, "Current Link Speed: 5.0GT/s\n");
		break;
	case 0x3:
		dev_info(pp->dev, "Current Link Speed: 8.0GT/s\n");
		break;
	case 0x4:
		dev_info(pp->dev, "Current Link Speed: 16.0GT/s\n");
		break;
	case 0x5:
		dev_info(pp->dev, "Current Link Speed: 32.0GT/s\n");
		break;
	default:
		dev_info(pp->dev, "Current Link Speed: not define(0x%x)\n", cur_link_speed);
		break;
	}

	/* Enable bus mastering and memory access */
	writel(0x7, pp->ctrl_base + TYPE1_STATUS_COMMAND_REG);

	/* Run hardware initialization */
	ret = pp->ops->hwinit(pp);
	if (ret) {
		dev_err(pp->dev, "hw init failed.\n");
		return ret;
	}

	/* Set memory limit and base registers */
	writel(0x0000FFF0, pp->ctrl_base + MEM_LIMIT_MEM_BASE_REG);
	writel(0x0000FFF0, pp->ctrl_base + PREF_MEM_LIMIT_PREF_MEM_BASE_REG);

	return 0;
}


static int rtd_pcie_probe(struct udevice *dev)
{
	struct rtd_pcie_port *pp = dev_get_priv(dev);
	int ret = 0;

	pp->dev = dev;
	pp->first_busno = dev_seq(dev);
	pp->info = (struct rtd_pcie_info *)dev_get_driver_data(dev);
	if (!pp->info) {
		dev_err(dev, "cannot get pcie info\n");
		return -EINVAL;
	}

	pp->ops = pp->info->ops;
	
	dev_dbg(dev, "host driver initial begin.\n");

	ret = pp->ops->get_resource(pp);
	if (ret) {
		printf("get resource failed.\n");
		goto failed;
	}

	ret = pp->ops->init(pp);
	if (ret) {
		dev_err(dev, "init failed.\n");
		goto deinit;
	}

	ret = pcie_link_init(pp);
	if (ret) {
		dev_err(dev, "link init failed.\n");
		goto failed;
	}
	//pp->dw.dbi_base = pp->ctrl_base;
	//pcie_dw_setup_host(&pp->dw);

	dev_info(dev, "host driver initial done.\n");

	return 0;

deinit:
	ret = pp->ops->deinit(pp);
	if (ret) {
		dev_err(pp->dev, "deinit failed.\n");
		return 0;
	}

failed:
	dev_info(pp->dev, "host driver initial failed.\n");
	return 0;
}

static int rtd1625_pcie0_get_res(struct rtd_pcie_port *pp)
{
	struct udevice *syscon_dev;
	int ret;

	pp->ctrl_base =	dev_read_addr_index_ptr(pp->dev, 0);
	if (!pp->ctrl_base) {
		printf("failed to get ctrl address\n");
		return -EINVAL;
	}

	pp->speed_mode = dev_read_u32_default(pp->dev, "speed-mode", 0);
	pp->debug_mode = dev_read_u32_default(pp->dev, "debug-mode", 0);

	/* Get SCPU wrapper syscon */
	ret = uclass_get_device_by_phandle(UCLASS_SYSCON, pp->dev,
					   "syscon-scpu-wrapper", &syscon_dev);
	if (ret) {
		dev_err(pp->dev, "Failed to get SCPU wrapper syscon: %d\n", ret);
		return ret;
	}
	pp->scpu_wrapper_base = syscon_get_regmap(syscon_dev);
	if (!pp->scpu_wrapper_base) {
		dev_err(pp->dev, "Failed to get SCPU wrapper regmap\n");
		return -EINVAL;
	}

	/* Get main2_misc (m2tmx) syscon for PHY mux control */
	ret = uclass_get_device_by_phandle(UCLASS_SYSCON, pp->dev,
					   "syscon-iso", &syscon_dev);
	if (ret) {
		dev_err(pp->dev, "Failed to get iso syscon: %d\n", ret);
		return ret;
	}
	pp->iso_base = syscon_get_regmap(syscon_dev);
	if (!pp->iso_base) {
		dev_err(pp->dev, "Failed to get iso regmap\n");
		return -EINVAL;
	}

	ret = gpio_request_by_name(pp->dev, "perst-gpios", 0, &pp->perst_gpio,
				   GPIOD_IS_OUT);
	if (ret) {
		dev_err(pp->dev, "Failed to get PERST GPIO: %d\n", ret);
		return ret;
	}

	ret = dm_gpio_set_value(&pp->perst_gpio, 0);
	if (ret) {
		dev_err(pp->dev, "Failed to assert PERST#\n");
		return ret;
	}

	ret = gpio_request_by_name(pp->dev, "device-power-gpios", 0, &pp->device_power_gpio,
				   GPIOD_IS_OUT);
	if (ret) {
		dev_info(pp->dev, "device power gpio missing or invalid, skipping\n");
	} else {
		ret = dm_gpio_set_value(&pp->device_power_gpio, 0);
		if (ret) {
			dev_err(pp->dev, "Failed to assert device power gpio\n");
			return ret;
		}
	}

	ret = generic_phy_get_by_index(pp->dev, 0, &pp->pcie_phy);
	if (ret) {
		dev_err(pp->dev, "Failed to get PHY: %d\n", ret);
		return ret;
	}

	/* Create regmap for CRT clock/reset controller (0x98000000, 0x1000) */
	ret = regmap_init_mem_range(dev_ofnode(pp->dev), 0x98000000, 0x1000,
				     &pp->crt);
	if (ret) {
		dev_err(pp->dev, "Failed to create CRT regmap: %d\n", ret);
		return ret;
	}

	return 0;
}

static int rtd1625_pcie1_device_power_get(struct rtd_pcie_port *pp)
{
	int ret;
	uint8_t val;

	ret = uclass_get_device_by_phandle(UCLASS_I2C_GENERIC, pp->dev, "device-power-supply", &pp->device_power_supply);
	if (ret == 0) {
		 ret = dm_i2c_read(pp->device_power_supply, 0x0, &val, 1);
		 if (ret == 0) {
			dev_info(pp->dev, "PMIC is detected. Using PMIC to set device power\n");
			pp->device_pwr_type = PCIE_PWR_TYPE_PMIC;
			return 0;
		 }
	}

	ret = gpio_request_by_name(pp->dev, "device-power-gpios", 0, &pp->device_power_gpio,
					   GPIOD_IS_OUT);
	if (ret) {
		dev_info(pp->dev, "device power gpio missing or invalid, skipping\n");
	} else {
		pp->device_pwr_type = PCIE_PWR_TYPE_GPIO;
		ret = dm_gpio_set_value(&pp->device_power_gpio, 0);
		if (ret) {
			dev_err(pp->dev, "Failed to assert device power gpio\n");
			return ret;
		}
	}
}

static int rtd1625_pcie1_get_res(struct rtd_pcie_port *pp)
{
	struct udevice *syscon_dev;
	int ret;

	pp->ctrl_base =	dev_read_addr_index_ptr(pp->dev, 0);
	if (!pp->ctrl_base) {
		printf("failed to get ctrl address\n");
		return -EINVAL;
	}

	pp->speed_mode = dev_read_u32_default(pp->dev, "speed-mode", 0);
	pp->debug_mode = dev_read_u32_default(pp->dev, "debug-mode", 0);

	/* Get SCPU wrapper syscon */
	ret = uclass_get_device_by_phandle(UCLASS_SYSCON, pp->dev,
					   "syscon-scpu-wrapper", &syscon_dev);
	if (ret) {
		dev_err(pp->dev, "Failed to get SCPU wrapper syscon: %d\n", ret);
		return ret;
	}
	pp->scpu_wrapper_base = syscon_get_regmap(syscon_dev);
	if (!pp->scpu_wrapper_base) {
		dev_err(pp->dev, "Failed to get SCPU wrapper regmap\n");
		return -EINVAL;
	}

	/* Get main2_misc (m2tmx) syscon for PHY mux control */
	ret = uclass_get_device_by_phandle(UCLASS_SYSCON, pp->dev,
					   "syscon-iso", &syscon_dev);
	if (ret) {
		dev_err(pp->dev, "Failed to get iso syscon: %d\n", ret);
		return ret;
	}
	pp->iso_base = syscon_get_regmap(syscon_dev);
	if (!pp->iso_base) {
		dev_err(pp->dev, "Failed to get iso regmap\n");
		return -EINVAL;
	}

	ret = gpio_request_by_name(pp->dev, "perst-gpios", 0, &pp->perst_gpio,
				   GPIOD_IS_OUT);
	if (ret) {
		dev_err(pp->dev, "Failed to get PERST GPIO: %d\n", ret);
		return ret;
	}

	ret = dm_gpio_set_value(&pp->perst_gpio, 0);
	if (ret) {
		dev_err(pp->dev, "Failed to assert PERST#\n");
		return ret;
	}

	ret = rtd1625_pcie1_device_power_get(pp);
	if (ret) {
		dev_err(pp->dev, "Failed to get device power control\n");
		return ret;
	}

	ret = generic_phy_get_by_index(pp->dev, 0, &pp->pcie_phy);
	if (ret) {
		dev_err(pp->dev, "Failed to get PHY: %d\n", ret);
		return ret;
	}

	/* Create regmap for CRT clock/reset controller (0x98000000, 0x1000) */
	ret = regmap_init_mem_range(dev_ofnode(pp->dev), 0x98000000, 0x1000,
				     &pp->crt);
	if (ret) {
		dev_err(pp->dev, "Failed to create CRT regmap: %d\n", ret);
		return ret;
	}

	return 0;
}

static int rtd1625_pcie0_powercut(struct rtd_pcie_port *pp, int power)
{

	if (power)
		regmap_update_bits(pp->iso_base, ISO_POWERCUT, PCIE0_POWERCUT_BIT, PCIE0_POWERCUT_BIT);
	else
		regmap_update_bits(pp->iso_base, ISO_POWERCUT, PCIE0_POWERCUT_BIT, 0);

	return 0;

}

static int rtd1625_pcie0_init(struct rtd_pcie_port *pp)
{
	int ret = 0;

	ret = rtd1625_pcie0_powercut(pp, 1);
	if (ret) {
		dev_err(pp->dev, "failed to power on pcie0\n");
		return ret;
	}

	if (dm_gpio_is_valid(&pp->device_power_gpio)) {
		ret = dm_gpio_set_value(&pp->device_power_gpio, 1);
		if (ret) {
			dev_err(pp->dev, "Failed to deassert device power gpio\n");
			return ret;
		}
	}
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_phy_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_phy_mdio_bit, false);
	if (ret)
		dev_err(pp->dev, "Failed to deassert init resets: %d\n", ret);

	ret = rtd_clk_update_bits_we(pp->crt, pp->info->clk_reg, pp->info->clk_bit, true);
	if (ret)
		dev_err(pp->dev, "Failed to enable clock: %d\n", ret);

	return 0;
}

static int rtd1625_pcie0_init2(struct rtd_pcie_port *pp)
{
	int ret = 0;

	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_core_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_power_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_stitch_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_nonstitch_bit, false);
	if (ret)
		dev_err(pp->dev, "Failed to deassert init2 resets: %d\n", ret);

	writel(0x060007A4, pp->ctrl_base + 0x8f8);
	writel(0x0E666060, pp->ctrl_base + 0x8fc);

	return 0;
}

static int rtd1625_pcie0_deinit(struct rtd_pcie_port *pp)
{
	int ret = 0;

	ret = rtd_clk_update_bits_we(pp->crt, pp->info->clk_reg, pp->info->clk_bit, false);
	if (ret)
		dev_err(pp->dev, "Failed to disabled clock: %d\n", ret);

	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_core_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_power_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_stitch_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_nonstitch_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_phy_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_phy_mdio_bit, true);
	if (ret)
		dev_err(pp->dev, "Failed to assert resets: %d\n", ret);

	if (dm_gpio_is_valid(&pp->device_power_gpio)) {
		ret = dm_gpio_set_value(&pp->device_power_gpio, 0);
		if (ret) {
			dev_err(pp->dev, "Failed to assert device power gpio\n");
			return ret;
		}
	}
	ret = rtd1625_pcie0_powercut(pp, 0);
	if (ret)
		dev_err(pp->dev, "failed to power off pcie0\n");

	return ret;
}

static int rtd1625_pcie1_powercut(struct rtd_pcie_port *pp, int power)
{
	if (power) {
		regmap_update_bits(pp->iso_base, ISO_POWERCUT, PCIE1_POWERCUT_BIT, PCIE1_POWERCUT_BIT);
		regmap_write(pp->iso_base, ISO_PCIE1_CTRL, 0x3e);
	} else {
		regmap_update_bits(pp->iso_base, ISO_POWERCUT, PCIE1_POWERCUT_BIT, 0);
		regmap_write(pp->iso_base, ISO_PCIE1_CTRL, 0x1);
	}
	return 0;

}

static int rtd1625_pcie1_init(struct rtd_pcie_port *pp)
{
	int ret = 0;
	uint8_t val;

	ret = rtd1625_pcie1_powercut(pp, 1);
	if (ret) {
		dev_err(pp->dev, "failed to power on pcie1\n");
		return ret;
	}

	if (pp->device_pwr_type == PCIE_PWR_TYPE_PMIC) {
		val = 0xff;
		ret = dm_i2c_write(pp->device_power_supply, 0x3000, &val, 1);
		if (ret) {
			dev_err(pp->dev, "Failed to deassert device power via pmic\n");
			return ret;
		}
	} else if (pp->device_pwr_type == PCIE_PWR_TYPE_GPIO) {
		if (dm_gpio_is_valid(&pp->device_power_gpio)) {
			ret = dm_gpio_set_value(&pp->device_power_gpio, 1);
			if (ret) {
				dev_err(pp->dev, "Failed to deassert device power gpio\n");
				return ret;
			}
		}
	}

	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_core_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_power_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_stitch_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_nonstitch_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_phy_bit, false);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_phy_mdio_bit, false);
	if (ret)
		dev_err(pp->dev, "Failed to deassert resets: %d\n", ret);

	ret = rtd_clk_update_bits_we(pp->crt, pp->info->clk_reg, pp->info->clk_bit, true);
	if (ret)
		dev_err(pp->dev, "Failed to enable clock: %d\n", ret);

	writel(0x0020, pp->ctrl_base + PCIE_PWR_CTR);

	ret = check_pipe_clock_ready(pp);
	if (ret)
		return ret;

	return 0;
}

static int rtd1625_pcie1_deinit(struct rtd_pcie_port *pp)
{
	int ret = 0;
	uint8_t val;

	ret = rtd_clk_update_bits_we(pp->crt, pp->info->clk_reg, pp->info->clk_bit, false);
	if (ret)
		dev_err(pp->dev, "Failed to disabled clock: %d\n", ret);

	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_core_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_power_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_stitch_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_nonstitch_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_phy_bit, true);
	ret |= rtd_rst_update_bits_we(pp->crt, pp->info->rst_reg, pp->info->rst_phy_mdio_bit, true);
	if (ret)
		dev_err(pp->dev, "Failed to assert resets: %d\n", ret);

	if (pp->device_pwr_type == PCIE_PWR_TYPE_PMIC) {
		val = 0xfd;
		ret = dm_i2c_write(pp->device_power_supply, 0x3000, &val, 1);
		if (ret) {
			dev_err(pp->dev, "Failed to assert device power via pmic\n");
			return ret;
		}
	} else if (pp->device_pwr_type == PCIE_PWR_TYPE_GPIO) {
		if (dm_gpio_is_valid(&pp->device_power_gpio)) {
			ret = dm_gpio_set_value(&pp->device_power_gpio, 0);
			if (ret) {
				dev_err(pp->dev, "Failed to assert device power gpio\n");
				return ret;
			}
		}
	}
	ret = rtd1625_pcie1_powercut(pp, 0);
	if (ret)
		dev_err(pp->dev, "failed to power off pcie1\n");

	return ret;
}

static int rtd1625_pcie0_hw_init(struct rtd_pcie_port *pp)
{
	u32 mmio_start, mmio_end, mmio_size;
	u32 regmap_st = 0;
	int timeout;
	int ret;
	struct pci_region *mem_region = NULL;
	struct udevice *ctlr = pci_get_controller(pp->dev);
	struct pci_controller *hose = dev_get_uclass_priv(ctlr);
	int i;

	dev_dbg(pp->dev, "Hardware initialization\n");

	/* Find memory region from PCI controller */
	for (i = 0; i < hose->region_count; i++) {
		if (hose->regions[i].flags == PCI_REGION_MEM) {
			mem_region = &hose->regions[i];
			break;
		}
	}

	if (!mem_region) {
		dev_err(pp->dev, "No memory region found\n");
		return -EINVAL;
	}

	mmio_start = mem_region->phys_start;
	mmio_size = mem_region->size;
	mmio_end = mmio_start + mmio_size - 1;

	dev_dbg(pp->dev, " MMIO region 0x%08x - 0x%08x\n", mmio_start, mmio_end);

	regmap_write(pp->scpu_wrapper_base, PCIE0_START, mmio_start);
	regmap_write(pp->scpu_wrapper_base, PCIE0_END, mmio_end);
	regmap_write(pp->scpu_wrapper_base, PCIE0_CTRL, 0x1);
	timeout = 500;
	while (regmap_st != 0x1 && timeout > 0) {
		regmap_read(pp->scpu_wrapper_base, PCIE0_STAT, &regmap_st);
		mdelay(1);
		timeout--;
	}
	if (timeout == 0) {
		dev_err(pp->dev, "failed to set mmio start/end\n");
		return -ETIMEDOUT;
	}

	writel(mmio_start, pp->ctrl_base + PCIE_DDR_START);
	writel(mmio_end, pp->ctrl_base + PCIE_DDR_END);
	writel(readl(pp->ctrl_base + PCIE_SYS_CTR) | 0x24000000, pp->ctrl_base + PCIE_SYS_CTR);
	writel(mmio_start, pp->ctrl_base + PCI_BASE_2);

	writel(0x0, pp->ctrl_base + PCIE_SERVICE_REGION);

	dev_dbg(pp->dev, "Hardware initialization complete\n");

	return 0;
}

static int rtd1625_pcie1_hw_init(struct rtd_pcie_port *pp)
{
	u32 mmio_start, mmio_end, mmio_size;
	u32 regmap_st = 0;
	int timeout;
	int err;
	struct pci_region *mem_region = NULL;
	struct udevice *ctlr = pci_get_controller(pp->dev);
	struct pci_controller *hose = dev_get_uclass_priv(ctlr);
	int i;

	dev_dbg(pp->dev, "Hardware initialization\n");

	/* Find memory region from PCI controller */
	for (i = 0; i < hose->region_count; i++) {
		if (hose->regions[i].flags == PCI_REGION_MEM) {
			mem_region = &hose->regions[i];
			break;
		}
	}

	if (!mem_region) {
		dev_err(pp->dev, "No memory region found\n");
		return -EINVAL;
	}

	mmio_start = mem_region->phys_start;
	mmio_size = mem_region->size;
	mmio_end = mmio_start + mmio_size - 1;

	dev_dbg(pp->dev, " MMIO region 0x%08x - 0x%08x\n", mmio_start, mmio_end);

	regmap_write(pp->scpu_wrapper_base, PCIE1_START, mmio_start);
	regmap_write(pp->scpu_wrapper_base, PCIE1_END, mmio_end);
	regmap_write(pp->scpu_wrapper_base, PCIE1_CTRL, 0x1);
	timeout = 500;
	while (!(regmap_st & 0x1) && timeout > 0) {
		regmap_read(pp->scpu_wrapper_base, PCIE1_STAT, &regmap_st);
		mdelay(1);
		timeout--;
	}
	if (timeout == 0) {
		dev_err(pp->dev, "failed to set mmio start/end\n");
		return -EINVAL;
	}

	writel(mmio_start, pp->ctrl_base + PCIE_DDR_START);
	writel(mmio_end, pp->ctrl_base + PCIE_DDR_END);
	writel(readl(pp->ctrl_base + PCIE_SYS_CTR) | 0x24000000, pp->ctrl_base + PCIE_SYS_CTR);
	writel(mmio_start, pp->ctrl_base + PCI_BASE_2);

	writel(0x0, pp->ctrl_base + PCIE_SERVICE_REGION);

	return 0;
}


static const struct rtd_pcie_ops rtd1625_pcie0_ops = {
	.name = "pcie0",
	.get_resource = rtd1625_pcie0_get_res,
	.init = rtd1625_pcie0_init,
	.init2 = rtd1625_pcie0_init2,
	.deinit = rtd1625_pcie0_deinit,
	.hwinit = rtd1625_pcie0_hw_init,
};

static const struct rtd_pcie_info rtd1625_pcie0_info = {
	.clk_reg = RTD1625_PCIE0_CLK_REG,
	.clk_bit = RTD1625_PCIE0_CLK_BIT,
	.rst_reg = RTD1625_PCIE0_RSTN_REG,
	.rst_bit = RTD1625_PCIE0_RSTN_BIT,
	.rst_core_bit = RTD1625_PCIE0_RSTN_CORE_BIT,
	.rst_power_bit = RTD1625_PCIE0_RSTN_POWER_BIT,
	.rst_stitch_bit = RTD1625_PCIE0_RSTN_STITCH_BIT,
	.rst_phy_bit = RTD1625_PCIE0_RSTN_PHY_BIT,
	.rst_phy_mdio_bit = RTD1625_PCIE0_RSTN_PHY_MDIO_BIT,
	.rst_nonstitch_bit = RTD1625_PCIE0_RSTN_NONSTITCH_BIT,
	.ops = &rtd1625_pcie0_ops,
};


static const struct rtd_pcie_ops rtd1625_pcie1_ops = {
	.name = "pcie1",
	.get_resource = rtd1625_pcie1_get_res,
	.init = rtd1625_pcie1_init,
	.deinit = rtd1625_pcie1_deinit,
	.hwinit = rtd1625_pcie1_hw_init,
};

static const struct rtd_pcie_info rtd1625_pcie1_info = {
	.clk_reg = RTD1625_PCIE1_CLK_REG,
	.clk_bit = RTD1625_PCIE1_CLK_BIT,
	.rst_reg = RTD1625_PCIE1_RSTN_REG,
	.rst_bit = RTD1625_PCIE1_RSTN_BIT,
	.rst_core_bit = RTD1625_PCIE1_RSTN_CORE_BIT,
	.rst_power_bit = RTD1625_PCIE1_RSTN_POWER_BIT,
	.rst_stitch_bit = RTD1625_PCIE1_RSTN_STITCH_BIT,
	.rst_phy_bit = RTD1625_PCIE1_RSTN_PHY_BIT,
	.rst_phy_mdio_bit = RTD1625_PCIE1_RSTN_PHY_MDIO_BIT,
	.rst_nonstitch_bit = RTD1625_PCIE1_RSTN_NONSTITCH_BIT,
	.ops = &rtd1625_pcie1_ops,
};


static const struct dm_pci_ops rtd_pcie_cfg_ops = {
	.read_config	= rtd_pcie_read_config,
	.write_config	= rtd_pcie_write_config,
};

static const struct udevice_id rtd_pcie_ids[] = {
	{.compatible = "realtek,rtd1625-pcie-slot0", .data = &rtd1625_pcie0_info},
	{.compatible = "realtek,rtd1625-pcie-slot1", .data = &rtd1625_pcie1_info},
	{ }
};

U_BOOT_DRIVER(rtd_dw_pcie) = {
	.name			= "pcie_dw_realtek",
	.id			= UCLASS_PCI,
	.of_match		= rtd_pcie_ids,
	.ops			= &rtd_pcie_cfg_ops,
	.probe			= rtd_pcie_probe,
	.priv_auto		= sizeof(struct rtd_pcie_port),
};
