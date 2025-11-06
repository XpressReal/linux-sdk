// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek DHC PCIe IOMMU driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 */

#include <linux/dma-direct.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_iommu.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#define RTD_PCIE_IOMMU_PGSIZES (SZ_4K | SZ_64K)
#define MMU_CTRL 0xE00
#define MMU_VA_START 0xE04
#define MMU_VA_END 0xE08
#define MMU_FAULT_RETRY_COUNT 0xE18
#define MMU_PGTABLE_BASE 0xE0C
#define MMU_TLB_INVAL 0xE70
#define MMU_TLB_VALID_STATUS 0xE78
#define MMU_TLB_TABLE_BASE 0xE80

struct rtd_pcie_iommu {
	struct device *dev;
	struct regmap *base;
	struct iommu_device iommu;
	struct list_head node;
	struct iommu_domain *domain;
	struct iommu_group *group;
	dma_addr_t	dma_start;
	u32		size;
	dma_addr_t pgtable_paddr;
	u32 *pgtable;
};

struct rtd_pcie_iommu_domain {
	struct list_head iommus;
	struct iommu_domain domain;
	struct rtd_pcie_iommu *iommu_dev;
};

static struct rtd_pcie_iommu_domain *to_rtd_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct rtd_pcie_iommu_domain, domain);
}

static struct iommu_domain *rtd_pcie_iommu_domain_alloc(unsigned type)
{
	struct rtd_pcie_iommu_domain *rtd_domain;

	rtd_domain = kzalloc(sizeof(*rtd_domain), GFP_KERNEL);
	if (!rtd_domain)
		return NULL;

	INIT_LIST_HEAD(&rtd_domain->iommus);

	rtd_domain->domain.geometry.aperture_start = 0;
	rtd_domain->domain.geometry.aperture_end   = DMA_BIT_MASK(33);
	rtd_domain->domain.geometry.force_aperture = true;

	return &rtd_domain->domain;

}

static void rtd_pcie_iommu_domain_free(struct iommu_domain *domain)
{
	struct rtd_pcie_iommu_domain *rtd_domain = to_rtd_domain(domain);

	WARN_ON(!list_empty(&rtd_domain->iommus));

	kfree(rtd_domain);
}

static int rtd_pcie_iommu_attach_dev(struct iommu_domain *domain,
		struct device *dev)
{
	struct rtd_pcie_iommu *rtd_iommu;
	struct rtd_pcie_iommu_domain *rtd_domain = to_rtd_domain(domain);
	int ret = 0;
	u32 *pgtable;

	rtd_iommu = dev_iommu_priv_get(dev);
	if (!rtd_iommu)
		return 0;
	pgtable = rtd_iommu->pgtable;

	if (rtd_iommu->domain == domain)
		return 0;

	rtd_iommu->domain = domain;
	rtd_domain->iommu_dev = rtd_iommu;

	rtd_iommu->dma_start = dev->dma_range_map->dma_start;
	rtd_iommu->size = dev->dma_range_map->size;

	list_add_tail(&rtd_iommu->node, &rtd_domain->iommus);
	regmap_write(rtd_iommu->base, MMU_CTRL, BIT(0) | BIT(1));
	regmap_write(rtd_iommu->base, MMU_VA_START, rtd_iommu->dma_start >> 4);
	regmap_write(rtd_iommu->base, MMU_VA_END, (rtd_iommu->dma_start + rtd_iommu->size) >> 4);
	regmap_write(rtd_iommu->base, MMU_PGTABLE_BASE, BIT(0) | BIT(1));
	regmap_write(rtd_iommu->base, MMU_FAULT_RETRY_COUNT, 2);

	return ret;
}

static void rtd_pcie_iommu_detach_dev(struct iommu_domain *domain,
				   struct device *dev)
{

}

static int rtd_pcie_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t pa, size_t len, int prot, gfp_t gfp)
{

	struct rtd_pcie_iommu_domain *rtd_domain = to_rtd_domain(domain);
	struct rtd_pcie_iommu *rtd_iommu = rtd_domain->iommu_dev;
	u32 *pgtable = rtd_iommu->pgtable;
	int i = 0;
	u64 base = (iova - rtd_iommu->dma_start) / PAGE_SIZE;
	u32 loop = len / PAGE_SIZE;

	dev_dbg(rtd_iommu->dev, "%s: domain(%p): iova=%lx, pa=%llx, len=%zx\n",
		__func__, domain, iova, pa, len);

	for (i = 0; i < loop; i++)
		pgtable[base + i] = ((((pa + i * 0x1000) & GENMASK(34, 12))) >> 3) | 0xff;

	return 0;
}

static size_t rtd_pcie_iommu_unmap(struct iommu_domain *domain, unsigned long iova,
			      size_t len, struct iommu_iotlb_gather *gather)
{
	struct rtd_pcie_iommu_domain *rtd_domain = to_rtd_domain(domain);
	struct rtd_pcie_iommu *rtd_iommu = rtd_domain->iommu_dev;
	u32 *pgtable = rtd_iommu->pgtable;
	int i = 0;
	u64 base = (iova - rtd_iommu->dma_start) / PAGE_SIZE;
	u32 loop = len / PAGE_SIZE;
	u64 tlb_entry;
	u32 tlb_entry_reg;
	u32 invalid_map = 0;
	u32 valid_map = 0;

	dev_dbg(rtd_iommu->dev, "%s: domain(%p): iova=%lx, len=%zx\n", __func__, domain, iova, len);

	for (i = 0; i < loop; i++)
		pgtable[base + i] = 0x0;
	regmap_read(rtd_iommu->base, MMU_TLB_VALID_STATUS, &valid_map);
	for (i = 0; i < 32; i++) {
		if (!(valid_map & BIT(i)))
			continue;
		regmap_read(rtd_iommu->base, MMU_TLB_TABLE_BASE + i * 0x4, &tlb_entry_reg);
		tlb_entry = tlb_entry_reg << 4;
		if ((tlb_entry >= (iova & GENMASK(34, 17))) &&
		    (tlb_entry <= ((iova + len) & GENMASK(34, 17))))
			invalid_map |= BIT(i);
	}

	if (invalid_map)
		regmap_write(rtd_iommu->base, MMU_TLB_INVAL, invalid_map);

	return len;
}

static phys_addr_t rtd_pcie_iommu_iova_to_phys(struct iommu_domain *domain, dma_addr_t va)
{
	struct rtd_pcie_iommu_domain *rtd_domain = to_rtd_domain(domain);
	struct rtd_pcie_iommu *rtd_iommu = rtd_domain->iommu_dev;
	u32 *pgtable = rtd_iommu->pgtable;
	u64 base = (va - rtd_iommu->dma_start) / PAGE_SIZE;
	phys_addr_t paddr;

	paddr = (pgtable[base] & GENMASK(31, 9)) << 3;

	dev_dbg(rtd_iommu->dev, "%s: domain(%p): va=%pad   pa:%llx\n", __func__, domain, &va, paddr);

	return paddr;
}

static struct iommu_device *rtd_pcie_iommu_probe_device(struct device *dev)
{
	struct rtd_pcie_iommu *rtd_iommu;

	rtd_iommu = dev_iommu_priv_get(dev);
	if (!rtd_iommu)
		return ERR_PTR(-ENODEV);

	return &rtd_iommu->iommu;
}

static void rtd_pcie_iommu_release_device(struct device *dev)
{

}

static struct iommu_group *rtd_pcie_iommu_device_group(struct device *dev)
{
	struct rtd_pcie_iommu *rtd_pcie_iommu = dev_iommu_priv_get(dev);

	return rtd_pcie_iommu ? iommu_group_ref_get(rtd_pcie_iommu->group) : NULL;
}

static int rtd_pcie_iommu_of_xlate(struct device *dev,
			     struct of_phandle_args *args)
{
	struct platform_device *pdev;
	struct rtd_pcie_iommu *iommu;

	if (args->args_count != 1) {
		dev_err(dev, "%s: args_count=%d\n", __func__, args->args_count);
		return -EINVAL;
	}

	pdev = of_find_device_by_node(args->np);
	if (!pdev) {
		dev_err(dev, "%s: failed to find device\n", __func__);
		return -ENODEV;
	}

	iommu = platform_get_drvdata(pdev);
	if (!iommu) {
		dev_err(dev, "%s: failed to get iommu device\n", __func__);
		of_node_put(args->np);
		return -EINVAL;
	}

	dev_iommu_priv_set(dev, iommu);

	pr_err("%s: dev=%s, args->args[0]=%d\n", __func__, dev_name(dev), args->args[0]);

	return 0;
}

/*static int rtd_pcie_iommu_def_domain_type(struct device *dev)
 * {
 *	return IOMMU_DOMAIN_DMA;
 * }
 */

const struct iommu_ops rtd_pcie_iommu_ops = {
	.domain_alloc    = rtd_pcie_iommu_domain_alloc,
	.domain_free     = rtd_pcie_iommu_domain_free,
	.attach_dev      = rtd_pcie_iommu_attach_dev,
	.detach_dev      = rtd_pcie_iommu_detach_dev,
	.map             = rtd_pcie_iommu_map,
	.unmap           = rtd_pcie_iommu_unmap,
	.iova_to_phys    = rtd_pcie_iommu_iova_to_phys,
	.probe_device    = rtd_pcie_iommu_probe_device,
	.release_device  = rtd_pcie_iommu_release_device,
	.device_group    = rtd_pcie_iommu_device_group,
	.of_xlate        = rtd_pcie_iommu_of_xlate,
	//.def_domain_type = rtd_pcie_iommu_def_domain_type,
	.pgsize_bitmap   = RTD_PCIE_IOMMU_PGSIZES,
};

static int rtd_pcie_iommu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtd_pcie_iommu *rtd_iommu;
	int pgtable_size;
	int ret = 0;
	int i = 0;

	if (of_property_read_u32(dev->of_node, "pgtable-size", &pgtable_size))
		dev_err(dev, "failed to read page table size\n");

	rtd_iommu = devm_kzalloc(dev, sizeof(*rtd_iommu), GFP_KERNEL);
	if (!rtd_iommu)
		return -ENODEV;

	rtd_iommu->dev = dev;
	platform_set_drvdata(pdev, rtd_iommu);

	rtd_iommu->base = syscon_regmap_lookup_by_phandle(dev->of_node, "syscon");
	if (IS_ERR_OR_NULL(rtd_iommu->base))
		return -EINVAL;

	rtd_iommu->base = of_iomap(dev->of_node, 0);
	if (!rtd_iommu->base) {
		dev_err(dev, "ioremap rtd pcie mmio failed\n");
		return -ENOMEM;
	}

	rtd_iommu->group = iommu_group_alloc();
	if (IS_ERR(rtd_iommu->group))
		return PTR_ERR(rtd_iommu->group);

	ret = iommu_device_sysfs_add(&rtd_iommu->iommu, dev, NULL, dev_name(dev));
	if (ret)
		goto err_put_group;

	ret = iommu_device_register(&rtd_iommu->iommu, &rtd_pcie_iommu_ops, dev);
	if (ret)
		goto err_remove_sysfs;

	if (!iommu_present(&pci_bus_type))
		bus_set_iommu(&pci_bus_type, &rtd_pcie_iommu_ops);

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret)
		dev_err(dev, "failed to assign reserved memory to device\n");

	rtd_iommu->pgtable = dma_alloc_coherent(&pdev->dev, pgtable_size, &rtd_iommu->pgtable_paddr, GFP_KERNEL);
	for (i = 0; i < pgtable_size; i++)
		rtd_iommu->pgtable[i] = 0;

	return 0;

err_remove_sysfs:
	iommu_device_sysfs_remove(&rtd_iommu->iommu);

err_put_group:
	iommu_group_put(rtd_iommu->group);

	return ret;
}

static const struct of_device_id rtd_pcie_iommu_of_match[] = {
	{ .compatible = "realtek,rtd-pcie-iommu" },
	{}
};
MODULE_DEVICE_TABLE(of, rtd_pcie_iommu_of_match);

static struct platform_driver rtd_pcie_iommu_driver = {
	.probe  = rtd_pcie_iommu_probe,
	.driver = {
		.name	= "rtd-pcie-iommu",
		.of_match_table = rtd_pcie_iommu_of_match,
	},
};
module_platform_driver(rtd_pcie_iommu_driver);

MODULE_DESCRIPTION("Realtek DHC PCIe IOMMU driver");
MODULE_LICENSE("GPL v2");
