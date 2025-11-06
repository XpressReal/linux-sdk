// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Realtek Autoread driver
 *
 * Copyright (C) 2025 Realtek Semiconductor Corporation.
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/kobject.h>
#include <linux/bitops.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/perf_event.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include "rtk_autoread.h"


#define READ_REG32(addr)		readl(addr)
#define WRITE_REG32(addr, val)	writel(val, addr)
#define AUTOREAD_READL(reg)	\
	readl((void __iomem *)(reg + (uintptr_t)(autoread_data->autoread_addr)))
#define AUTOREAD_WRITEL(val, reg)	\
	writel(val, (void __iomem *)(reg + (uintptr_t)(autoread_data->autoread_addr)))

#define to_rtk_autoread_data(obj)	container_of(obj, struct rtk_autoread_data, kobj)
#define to_kobj_attribute(x)		container_of(x, struct kobj_attribute, attr)
#define to_autoread_pmu(p)		container_of(p, struct autoread_pmu, pmu)

#define SET_SIZE		36
#define INA_DATA_SIZE		2
#define TIME_STAMP_SIZE	4
#define I2C_BUS_ID		7
#define INA_CPU_ID		4
#define INA_MAX_NUM		16

#define INA2XX_CONFIGURATION	0x00
#define INA2XX_SHUNT_VOLTAGE	0x01		/* readonly */
#define INA2XX_BUS_VOLTAGE	0x02		/* readonly */
#define INA2XX_POWER		0x03		/* readonly */
#define INA2XX_CURRENT	0x04		/* readonly */
#define INA2XX_CALIBRATION	0x05
#define INA219_CONFIG_DEFAULT	0x399F
#define INA226_CONFIG_DEFAULT	0x4027

#define AUTOREAD_PMU_MAX_COUNTERS	17
#define AUTOREAD_VAL_MASK		GENMASK(31, 0)
#define AUTOREAD_PMU_ADD_MASK		BIT(17)

enum ina2xx_ids { ina219, ina226 };

struct ina2xx_config {
	const char	*name;
	int		config_default;
	int		calibration_value;
	int		shunt_voltage_lsb;	/* nV */
	int		current_lsb;		/* μA */
	int		bus_voltage_lsb;	/* μV */
	int		power_lsb;		/* μW */
};

static const struct ina2xx_config ina2xx_configs[] = {
	[ina219] = {
		.name = "ina219",
		.config_default = INA219_CONFIG_DEFAULT,
		.calibration_value = 4096,
		.shunt_voltage_lsb = 10000,
		.current_lsb = 100,
		.bus_voltage_lsb = 4000,
		.power_lsb = 20000,
	},
	[ina226] = {
		.name = "ina226",
		.config_default = INA226_CONFIG_DEFAULT,
		.calibration_value = 5120,
		.shunt_voltage_lsb = 2500,
		.current_lsb = 100,
		.bus_voltage_lsb = 1250,
		.power_lsb = 25,
	},
};

struct rtk_autoread_data;
struct autoread_pmu {
	struct rtk_autoread_data	*autoread_data;
	struct pmu			pmu;
	char				*name;
	char				*cpuhp_name;
	/* active cpus */
	cpumask_t			cpus;
	/* cpuhp state for updating cpus */
	enum cpuhp_state		cpuhp_state;
	/* hlist node for cpu hot-plug */
	struct hlist_node		cpuhp;
	void            (*enable)(struct perf_event *event);
	void            (*disable)(struct perf_event *event);
	u64             (*read_counter)(struct perf_event *event);
	void            (*start)(struct autoread_pmu *pmu);
	void            (*stop)(struct autoread_pmu *pmu);
	DECLARE_BITMAP(used_mask, AUTOREAD_PMU_MAX_COUNTERS);
	void __iomem	*reg_base;
	void            *current_read_ptr;
	int		num_counters;
	u64		counter_mask;
	u64		counter_read_mask;
	unsigned long	read_bitmap;
};

struct rtk_autoread_data {
	struct device		*dev;
	struct kobject		kobj;
	const struct		ina2xx_config *ina_config;
	struct autoread_pmu	*pmu;
	dma_addr_t	dma_buff_addr_0;
	dma_addr_t	dma_buff_addr_1;
	void		*dma_buff_0;
	void		*dma_buff_1;
	void __iomem	*autoread_addr;
	struct regmap	*iso_base;
	int		irq;
	size_t		dma_size;
	int		cur_dma_buf;
};

static struct i2c_adapter *adap;
static struct kobject *autoread_kobj;
static unsigned long devices_bitmap = 0xFFFF;
static unsigned int loop_cnt = 0x10;
static unsigned int wait_time;
static unsigned int auto_en = 1;
static unsigned int ina_target = INA2XX_POWER;

/*
 * sysfs common attributes
 */
PMU_FORMAT_ATTR(event, "config:0-7");
static struct attribute *autoread_pmu_format_attrs[] = {
	&format_attr_event.attr,
	NULL,
};

static struct attribute_group autoread_pmu_format_attr_group = {
	.name = "format",
	.attrs = autoread_pmu_format_attrs,
};

static ssize_t cpumask_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct autoread_pmu *pmu = to_autoread_pmu(dev_get_drvdata(dev));

	return cpumap_print_to_pagebuf(true, buf, &pmu->cpus);
}

static DEVICE_ATTR_RO(cpumask);

static struct attribute *autoread_pmu_common_attrs[] = {
	&dev_attr_cpumask.attr,
	NULL,
};

static struct attribute_group autoread_pmu_common_attr_group = {
	.attrs = autoread_pmu_common_attrs,
};

/*
 * sysfs event attributes
 */
static ssize_t autoread_pmu_event_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct perf_pmu_events_attr *pmu_attr =
		container_of(attr, struct perf_pmu_events_attr, attr);

	return scnprintf(buf, PAGE_SIZE, "event=0x%04lx\n",
			 (unsigned long)pmu_attr->id);
}

#define AUTOREAD_PMU_EVENT_ATTR(_name, _config)				\
	(&((struct perf_pmu_events_attr) {					\
		.attr = __ATTR(_name, 0444, autoread_pmu_event_show, NULL),	\
		.id = _config,							\
	}).attr.attr)

static struct attribute *autoread_pmu_events_attrs[] = {
	AUTOREAD_PMU_EVENT_ATTR(timestamp,	0),
	AUTOREAD_PMU_EVENT_ATTR(pp0900_s0,	1),
	AUTOREAD_PMU_EVENT_ATTR(pp1050_mem_s3,	2),
	AUTOREAD_PMU_EVENT_ATTR(pp0500_mem_s0,	3),
	AUTOREAD_PMU_EVENT_ATTR(pp0800_s5,	4),
	AUTOREAD_PMU_EVENT_ATTR(ppdvfs_cpu_s0,	5),
	AUTOREAD_PMU_EVENT_ATTR(ppsram_s0,	6),
	AUTOREAD_PMU_EVENT_ATTR(pp1800_s5,	7),
	AUTOREAD_PMU_EVENT_ATTR(pp3300_s5,	8),
	AUTOREAD_PMU_EVENT_ATTR(pp5000_z0,	9),
	AUTOREAD_PMU_EVENT_ATTR(pp5000_s5,	10),
	AUTOREAD_PMU_EVENT_ATTR(pp3300_z1,	11),
	AUTOREAD_PMU_EVENT_ATTR(pp3300_wlan_x,	12),
	AUTOREAD_PMU_EVENT_ATTR(pp1800_ec_z1,	13),
	AUTOREAD_PMU_EVENT_ATTR(pp3300_ec_z1,	14),
	AUTOREAD_PMU_EVENT_ATTR(pp3300_gsc_z1,	15),
	AUTOREAD_PMU_EVENT_ATTR(ppvar_sys,	16),
	NULL,
};

static const struct attribute_group autoread_pmu_events_attr_group = {
	.name = "events",
	.attrs = autoread_pmu_events_attrs,
};

static const struct attribute_group *autoread_pmu_attr_groups[] = {
	&autoread_pmu_common_attr_group,
	&autoread_pmu_format_attr_group,
	&autoread_pmu_events_attr_group,
	NULL,
};

static void enable_i2c_int(struct rtk_autoread_data *autoread_data)
{
	AUTOREAD_WRITEL(0xC0, DMA_CTRL);
}

static void disable_i2c_int(struct rtk_autoread_data *autoread_data)
{
	AUTOREAD_WRITEL(0x80, DMA_CTRL);
}

static int rtk_autoread_i2c_read(unsigned char addr,
				  unsigned char reg,
				  unsigned char *val, int msg_len)
{
	int ret;
	struct i2c_msg msgs[2];

	msgs[0].addr = addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = (void *)&reg;

	msgs[1].addr = addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = msg_len;
	msgs[1].buf = val;

	ret = i2c_transfer(adap, msgs, 2);

	if (ret < 0)
		pr_err("Failed to do i2c read\n");

	return ret;
}

static int rtk_autoread_i2c_write(unsigned char addr,
				   unsigned char reg,
				   uint16_t val)
{
	int ret;
	unsigned char data[3];
	struct i2c_msg msg[1];

	data[0] = reg;
	data[1] = (val >> 8) & 0xFF;
	data[2] = val & 0xFF;

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = sizeof(data);
	msg[0].buf = (void *)data;

	ret = i2c_transfer(adap, msg, 1);

	if (ret < 0)
		pr_err("Failed to do i2c write\n");

	return ret;
}

static void rtk_autoread_ina_setting(struct rtk_autoread_data *autoread_data)
{
	int i;
	int calibration_value;
	unsigned int device_id = 0x40;

	enable_i2c_int(autoread_data);

	for (i = 0; i < INA_MAX_NUM; i++) {
		rtk_autoread_i2c_write(device_id, INA2XX_CONFIGURATION,
				       autoread_data->ina_config->config_default);

		calibration_value =
			(i != INA_CPU_ID) ? autoread_data->ina_config->calibration_value : 5689;
		rtk_autoread_i2c_write(device_id, INA2XX_CALIBRATION, calibration_value);

		device_id++;
	}

	disable_i2c_int(autoread_data);
}

static ssize_t devices_show(struct kobject *kobj,
			     struct kobj_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "0x%lx\n", devices_bitmap);
}

static ssize_t devices_store(struct kobject *kobj,
			      struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	if (kstrtoul(buf, 16, &devices_bitmap) < 0)
		return -EINVAL;

	if (devices_bitmap > 0xFFFF)
		return -EINVAL;

	return count;
}

static ssize_t loop_cnt_show(struct kobject *kobj,
			      struct kobj_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "0x%x\n", loop_cnt);
}

static ssize_t loop_cnt_store(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	if (kstrtouint(buf, 16, &loop_cnt) < 0)
		return -EINVAL;

	if (loop_cnt < 0x10)
		return -EINVAL;

	return count;
}

static ssize_t ina_target_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	return sprintf(buf, "0x%x\n", ina_target);
}

static ssize_t ina_target_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	if (kstrtouint(buf, 16, &ina_target) < 0)
		return -EINVAL;

	return count;
}

static ssize_t dma_wait_time_show(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   char *buf)
{
	return sprintf(buf, "0x%x\n", wait_time);
}

static ssize_t dma_wait_time_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	if (kstrtouint(buf, 16, &wait_time) < 0)
		return -EINVAL;

	return count;
}

static ssize_t auto_en_show(struct kobject *kobj,
			     struct kobj_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "%u\n", auto_en);
}

static int autoread_memory_setting(struct rtk_autoread_data *autoread_data)
{
	uint32_t dma_lsb, dma_msb;

	autoread_data->dma_size =
		(TIME_STAMP_SIZE + INA_DATA_SIZE * hweight16(devices_bitmap & 0xFFFF)) * loop_cnt;
	autoread_data->dma_buff_0 =
		dma_alloc_coherent(autoread_data->dev, autoread_data->dma_size * 2,
				   &autoread_data->dma_buff_addr_0, GFP_KERNEL);
	if (!autoread_data->dma_buff_0) {
		dev_err(autoread_data->dev, "Failed to allocate DMA buffer\n");
		return -ENOMEM;
	}

	autoread_data->dma_buff_1 = autoread_data->dma_buff_0 + autoread_data->dma_size;
	dma_lsb = (uint32_t)(autoread_data->dma_buff_addr_0 & 0xFFFFFFFF);
	dma_msb = (uint32_t)((autoread_data->dma_buff_addr_0 >> 32) & 0xFFFFFFFF);

	AUTOREAD_WRITEL(dma_lsb, DMA_LSB);
	AUTOREAD_WRITEL(dma_msb, DMA_MSB);

	return 0;
}


static void autoread_target_setting(struct rtk_autoread_data *autoread_data)
{
	int i;
	unsigned int device_id;
	const unsigned int base_set_ctrl_val = 0x31000;
	const unsigned int base_device_id = 0x40;
	unsigned int set_ctrl_addr = 0x0;
	unsigned int set_ctrl_val;

	for (i = 0; i < INA_MAX_NUM; i++) {
		if (!test_bit(i, &devices_bitmap))
			continue;

		device_id = base_device_id + i;
		set_ctrl_val = base_set_ctrl_val + device_id;
		AUTOREAD_WRITEL(set_ctrl_val, set_ctrl_addr);

		set_ctrl_addr += 0x4;
	}
}

static int autoread_target_i2c_read(struct rtk_autoread_data *autoread_data)
{
	int i, ret;
	unsigned char val[2];
	unsigned int device_id;
	const unsigned int base_device_id = 0x40;

	enable_i2c_int(autoread_data);

	for (i = 0; i < INA_MAX_NUM; i++) {
		if (!test_bit(i, &devices_bitmap))
			continue;

		device_id = base_device_id + i;

		ret = rtk_autoread_i2c_read(device_id, ina_target, val, 2);
		if (ret < 0)
			return ret;
	}

	disable_i2c_int(autoread_data);

	return 0;
}

static int autoread_setting(struct rtk_autoread_data *autoread_data)
{
	int ret = 0;

	ret = autoread_memory_setting(autoread_data);
	if (ret)
		return ret;

	autoread_target_setting(autoread_data);
	ret = autoread_target_i2c_read(autoread_data);
	if (ret)
		return ret;

	AUTOREAD_WRITEL(loop_cnt, DMA_LOOPCNT);
	AUTOREAD_WRITEL(wait_time, DMA_WAIT);

	return ret;
}

static int autoread_enable(struct rtk_autoread_data *autoread_data)
{
	int ret = 0;

	ret = autoread_setting(autoread_data);
	if (ret)
		pr_err("autoread enalbe failed with error code %d\n", ret);
	else {
		pm_runtime_get_sync(autoread_data->dev);
		AUTOREAD_WRITEL(0x3, DMA_CTRL);
	}

	return ret;
}
static void autoread_disable(struct rtk_autoread_data *autoread_data)
{
	AUTOREAD_WRITEL(0x2, DMA_CTRL);

	if (autoread_data->dma_buff_0) {
		dma_free_coherent(autoread_data->dev, autoread_data->dma_size * 2,
					autoread_data->dma_buff_0,
					autoread_data->dma_buff_addr_0);
		autoread_data->dma_buff_0 = NULL;
		autoread_data->pmu->current_read_ptr = NULL;
	}

	pm_runtime_put_sync(autoread_data->dev);
}

static ssize_t auto_en_store(struct kobject *kobj,
			      struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	struct rtk_autoread_data *autoread_data;
	uint32_t val;
	int ret;

	autoread_data = to_rtk_autoread_data(kobj);
	if (!autoread_data) {
		pr_err("Failed to get autoread data from kobject\n");
		return -EINVAL;
	}

	if (kstrtouint(buf, 10, &auto_en) < 0)
		return -EINVAL;

	val = AUTOREAD_READL(DMA_CTRL) & 0x1;
	if (val == 0 && auto_en == 1) {
		ret = autoread_enable(autoread_data);
		if (ret)
			return ret;
	} else if (val == 1 && auto_en == 0) {
		autoread_disable(autoread_data);
	} else {
		return -EINVAL;
	}

	return count;
}

static struct kobj_attribute devices_attribute =
	__ATTR(devices, 0664, devices_show, devices_store);
static struct kobj_attribute loop_cnt_attribute =
	__ATTR(loop_cnt, 0664, loop_cnt_show, loop_cnt_store);
static struct kobj_attribute ina_target_attribute =
	__ATTR(ina_target, 0664, ina_target_show, ina_target_store);
static struct kobj_attribute dma_wait_time_attribute =
	__ATTR(dma_wait_time, 0664, dma_wait_time_show, dma_wait_time_store);
static struct kobj_attribute auto_en_attribute =
	__ATTR(auto_en, 0664, auto_en_show, auto_en_store);

static struct attribute *attrs[] = {
	&devices_attribute.attr,
	&loop_cnt_attribute.attr,
	&ina_target_attribute.attr,
	&dma_wait_time_attribute.attr,
	&auto_en_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static ssize_t autoread_attr_show(struct kobject *kobj,
				   struct attribute *attr,
				   char *buf)
{
	struct kobj_attribute *attribute = to_kobj_attribute(attr);

	return attribute->show(kobj, attribute, buf);
}

static ssize_t autoread_attr_store(struct kobject *kobj,
				    struct attribute *attr,
				    const char *buf, size_t len)
{
	struct kobj_attribute *attribute = to_kobj_attribute(attr);

	return attribute->store(kobj, attribute, buf, len);
}

static const struct sysfs_ops autoread_sysfs_ops = {
	.show = autoread_attr_show,
	.store = autoread_attr_store,
};

static struct kobj_type rtk_autoread_ktype = {
	.sysfs_ops = &autoread_sysfs_ops,
};

static void __maybe_unused __get_dmabuf_content(struct rtk_autoread_data *autoread_data,
						 int buffer_flag)
{
	int i;
	int buf_len = autoread_data->dma_size / sizeof(uint32_t);
	uint32_t *buffer = !buffer_flag ? (uint32_t *)autoread_data->dma_buff_0 :
						(uint32_t *)autoread_data->dma_buff_1;

	pr_info("Contents of dma_buff_%d:\n", buffer_flag);
	for (i = 0; i < buf_len; i++)
		pr_info("0x%08x\n", buffer[i]);
}

static irqreturn_t rtk_autoread_irq(int irq, void *data)
{
	uint32_t val;
	struct platform_device *pdev = data;
	struct rtk_autoread_data *autoread_data = platform_get_drvdata(pdev);

	regmap_read(autoread_data->iso_base, 0x0, &val);
	if (val & I2C_7_INT_MASK) {
		val = AUTOREAD_READL(DMA_ST);

		if (!(val & AUTOREAD_INT_MASK))
			return IRQ_NONE;

		if (val & DMA_ST_FALL0)
			AUTOREAD_WRITEL(DMA_ST_FALL0, DMA_ST);

		if (val & DMA_ST_FALL1)
			AUTOREAD_WRITEL(DMA_ST_FALL1, DMA_ST);

		regmap_write(autoread_data->iso_base, 0x0, I2C_7_INT_MASK);
	}

	return IRQ_HANDLED;
}

static void autoread_pmu_free(struct autoread_pmu *pmu)
{
	kfree(pmu);
}

static void rtk_autoread_cleanup(struct rtk_autoread_data *autoread_data,
				   struct device *dev)
{
	if (!autoread_data)
		return;

	if (autoread_data->dma_buff_0)
		dma_free_coherent(dev, autoread_data->dma_size * 2,
				  autoread_data->dma_buff_0, autoread_data->dma_buff_addr_0);

	if (autoread_kobj)
		kobject_put(autoread_kobj);

	if (autoread_data->kobj.state_initialized)
		kobject_put(&autoread_data->kobj);

	if (adap) {
		enable_i2c_int(autoread_data);
		i2c_put_adapter(adap);
		disable_i2c_int(autoread_data);
	}

	if (autoread_data->pmu) {
		perf_pmu_unregister(&autoread_data->pmu->pmu);
		cpuhp_state_remove_instance(autoread_data->pmu->cpuhp_state,
					    &autoread_data->pmu->cpuhp);
		cpuhp_remove_multi_state(autoread_data->pmu->cpuhp_state);
		autoread_pmu_free(autoread_data->pmu);
		autoread_data->pmu = NULL;
	}

	devm_kfree(dev, autoread_data);

	pr_info("rtk autoread cleanup\n");
}

static void __maybe_unused __autoread_pmu_enable(struct autoread_pmu *pmu)
{
	;
}

static void __maybe_unused __autoread_pmu_disable(struct autoread_pmu *pmu)
{
	;
}

static void autoread_pmu_start(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	hwc->state = 0;

	local64_set(&hwc->prev_count, 0);
	perf_event_update_userpage(event);
	//autoread_pmu_enable_counter(event);
}

static void autoread_pmu_disable(struct pmu *pmu)
{
	;
}

static u64 __transform_raw_data(struct autoread_pmu *pmu, u64 val, int idx)
{
	struct rtk_autoread_data *autoread_data = pmu->autoread_data;
	const struct ina2xx_config *ina_config = autoread_data->ina_config;
	int power_lsb;
	int current_lsb;

	switch (ina_target) {
	case INA2XX_SHUNT_VOLTAGE:
		val *= ina_config->shunt_voltage_lsb;
		break;
	case INA2XX_BUS_VOLTAGE:
		val *= ina_config->bus_voltage_lsb;
		break;
	case INA2XX_POWER:
		current_lsb = ((idx - 1) != INA_CPU_ID) ? ina_config->current_lsb : 300;
		power_lsb = ina_config->power_lsb * current_lsb;
		val *= power_lsb;
		break;
	case INA2XX_CURRENT:
		current_lsb = ((idx - 1) != INA_CPU_ID) ? ina_config->current_lsb : 300;
		val *= current_lsb;
		break;
	}

	return val;
}

static u64 __read_dma_data(struct autoread_pmu *pmu, int idx)
{
	struct rtk_autoread_data *autoread_data = pmu->autoread_data;
	void *current_read_ptr = pmu->current_read_ptr;
	void *tmp_read_ptr;
	void *next_set_ptr;
	u64 val;
	uint32_t dma_total_size = autoread_data->dma_size * 2;
	int max_iterations = dma_total_size / SET_SIZE;
	int read_complete = (pmu->read_bitmap & pmu->counter_read_mask) == pmu->counter_read_mask;

	if (!current_read_ptr)
		current_read_ptr = autoread_data->dma_buff_0;

	tmp_read_ptr = current_read_ptr;
	if (pmu->read_bitmap & AUTOREAD_PMU_ADD_MASK || read_complete) {
		while (max_iterations--) {
			next_set_ptr = autoread_data->dma_buff_0
				+ ((tmp_read_ptr - autoread_data->dma_buff_0) + SET_SIZE)
				% dma_total_size;

			if (*(uint32_t *)next_set_ptr < (*(uint32_t *)tmp_read_ptr))
				break;

			tmp_read_ptr = next_set_ptr;
			if (read_complete)
				break;
		}

		current_read_ptr = tmp_read_ptr;
		pmu->read_bitmap = 0;
	}

	if (idx != 0) {
		tmp_read_ptr = current_read_ptr + 2 * (idx-1) + 4;
		val = *(uint16_t *)tmp_read_ptr;
		val = __transform_raw_data(pmu, val, idx);
	} else {
		val = *(uint32_t *)current_read_ptr;
	}

	pmu->read_bitmap |= BIT(idx);
	pmu->current_read_ptr = current_read_ptr;

	return val;
}

static u64 autoread_pmu_read_counter(struct autoread_pmu *pmu, int idx)
{
	u64 val = 0;

	if (idx >= pmu->num_counters)
		return -EINVAL;

	val = __read_dma_data(pmu, idx);

	return val;
}

static void __maybe_unused autoread_pmu_enable_counter(struct perf_event *event)
{
	;
}

static void __maybe_unused autoread_pmu_disable_counter(struct perf_event *event)
{
	;
}

static int autoread_pmu_get_counter(struct autoread_pmu *pmu,
				      struct perf_event *ev)
{
	int idx = ev->attr.config;

	if (idx >= pmu->num_counters)
		return -EINVAL;

	/* test already in use */
	if (!test_and_set_bit(idx, pmu->used_mask))
		return idx;

	return -EAGAIN;
}

static inline void autoread_pmu_clear_event_idx(struct autoread_pmu *pmu,
						  struct hw_perf_event *hwc)
{
	clear_bit(hwc->idx, pmu->used_mask);
}

static void autoread_pmu_event_update(struct perf_event *event)
{
	struct autoread_pmu *pmu = to_autoread_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	u64 prev, now, delta;

	/* ensure PMU is read only after DMA control has been initialized */
	if (!auto_en) {
		local64_set(&event->count, 0);
		return;
	}

	do {
		prev = local64_read(&hwc->prev_count);
		now = autoread_pmu_read_counter(pmu, hwc->idx);
	} while (local64_cmpxchg(&hwc->prev_count, prev, now) != prev);

	delta = now & pmu->counter_mask;
	if (hwc->idx == 0)
		local64_set(&event->count, delta);
	else
		local64_add(delta, &event->count);
}

/*
 * Implementation of abstract pmu functionality required by
 * the core perf events code.
 */
static void autoread_pmu_read(struct perf_event *event)
{
	autoread_pmu_event_update(event);
}

static void autoread_pmu_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (hwc->state & PERF_HES_STOPPED)
		return;

	//autoread_pmu_disable_counter(event);
	autoread_pmu_event_update(event);

	hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
}

static int autoread_pmu_add(struct perf_event *event, int flags)
{
	struct autoread_pmu *pmu = to_autoread_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx;

	/* If we don't have a space for the counter then finish early. */
	idx = autoread_pmu_get_counter(pmu, event);
	if (idx < 0)
		return idx;

	/*
	 * If there is an event in the counter we are going to use then make
	 * sure it is disabled.
	 */
	hwc->idx = idx;
	//autoread_pmu_disable_counter(event);

	hwc->state = PERF_HES_UPTODATE | PERF_HES_STOPPED;
	if (flags & PERF_EF_START)
		autoread_pmu_start(event, flags);

	/* Propagate our changes to the userspace mapping. */
	perf_event_update_userpage(event);


	/* Indicate pmu add happend */
	pmu->read_bitmap = AUTOREAD_PMU_ADD_MASK;
	pmu->counter_read_mask |= BIT(hwc->config);

	return 0;
}

static void autoread_pmu_del(struct perf_event *event, int flags)
{
	struct autoread_pmu *pmu = to_autoread_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	autoread_pmu_stop(event, flags | PERF_EF_UPDATE);
	autoread_pmu_clear_event_idx(pmu, hwc);

	perf_event_update_userpage(event);

	/* Clear the allocated counter */
	hwc->idx = -1;

	pmu->counter_read_mask &= ~BIT(hwc->config);
}

static int validate_event(struct pmu *pmu, struct perf_event *event,
			    int *counters)
{
	/* Don't allow groups with mixed PMUs, except for s/w events */
	if (is_software_event(event))
		return 0;

	/* Reject groups spanning multiple HW PMUs. */
	if (event->pmu != pmu)
		return -EINVAL;

	*counters = *counters + 1;
	return 0;
}

static int validate_group(struct perf_event *event)
{
	struct perf_event *sibling, *leader = event->group_leader;
	int counters = 0;

	if (validate_event(event->pmu, leader, &counters))
		return -EINVAL;

	for_each_sibling_event(sibling, event->group_leader) {
		if (validate_event(event->pmu, sibling, &counters))
			return -EINVAL;
	}

	if (validate_event(event->pmu, event, &counters))
		return -EINVAL;

	return 0;
}

static int autoread_pmu_event_init(struct perf_event *event)
{
	struct autoread_pmu *pmu = to_autoread_pmu(event->pmu);
	struct perf_event_attr *attr = &event->attr;
	struct hw_perf_event *hwc = &event->hw;

	/* This is, of course, deeply driver-specific */
	if (attr->type != event->pmu->type)
		return -ENOENT;

	if (attr->exclude_idle)
		return -EOPNOTSUPP;

	if (is_sampling_event(event)) {
		pr_debug("Sampling not supported\n");
		return -EOPNOTSUPP;
	}

	if (event->attach_state & PERF_ATTACH_TASK) {
		pr_debug("Per-task mode not supported\n");
		return -EOPNOTSUPP;
	}

	hwc->idx = -1;
	hwc->config = attr->config;

	if (event->group_leader != event) {
		if (validate_group(event) != 0)
			return -EINVAL;
	}

	event->cpu = cpumask_first(&pmu->cpus);
	return 0;
}

static void autoread_pmu_enable(struct pmu *pmu)
{
	;
}

static int __cpu_offline(unsigned int cpu, struct hlist_node *node)
{
	struct autoread_pmu *pmu = hlist_entry_safe(node, struct autoread_pmu,
						  cpuhp);
	unsigned int target;

	if (!cpumask_test_and_clear_cpu(cpu, &pmu->cpus))
		return 0;

	target = cpumask_any_but(cpu_online_mask, cpu);
	if (target >= nr_cpu_ids)
		return 0;

	cpumask_set_cpu(target, &pmu->cpus);
	return 0;
}

static void __pmu_init(struct autoread_pmu *pmu)
{
	pmu->pmu = (struct pmu){
		.module		= THIS_MODULE,
		.task_ctx_nr    = perf_invalid_context,
		.pmu_enable	= autoread_pmu_enable,
		.pmu_disable	= autoread_pmu_disable,
		.event_init	= autoread_pmu_event_init,
		.add		= autoread_pmu_add,
		.del		= autoread_pmu_del,
		.start		= autoread_pmu_start,
		.stop		= autoread_pmu_stop,
		.read		= autoread_pmu_read,
		.attr_groups	= autoread_pmu_attr_groups,
		.capabilities	= PERF_PMU_CAP_NO_EXCLUDE |
				  PERF_PMU_CAP_NO_INTERRUPT,
	};
}

static int __autoread_pmu_init(struct autoread_pmu *pmu)
{
	pmu->reg_base = pmu->autoread_data->autoread_addr;
	pmu->num_counters = AUTOREAD_PMU_MAX_COUNTERS;
	pmu->counter_mask = AUTOREAD_VAL_MASK;
	pmu->counter_read_mask = 0;

	/* set start performance trace bit */
	pmu->name = "rtk_1625_autoread_pmu";
	pmu->cpuhp_name = "perf/rtk_1625_autoread_pmu:online";

	return 0;
}

static int autoread_pmu_init(struct rtk_autoread_data *autoread_data)
{
	struct autoread_pmu *pmu;
	int ret;

	pmu = kzalloc(sizeof(*pmu), GFP_KERNEL);
	if (unlikely(!pmu)) {
		pr_err("failed to allocate PMU device!\n");
		return -ENOMEM;
	}

	pmu->autoread_data = autoread_data;
	__pmu_init(pmu);
	ret = __autoread_pmu_init(pmu);

	if (ret) {
		pr_err("OF: failed to probe PMU!\n");
		goto out_free;
	}

	pmu->cpuhp_state = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN,
						   pmu->cpuhp_name, NULL,
						   __cpu_offline);
	if (pmu->cpuhp_state < 0)
		goto out_free;

	cpumask_set_cpu(get_cpu(), &pmu->cpus);
	cpuhp_state_add_instance_nocalls(pmu->cpuhp_state, &pmu->cpuhp);
	put_cpu();

	ret = perf_pmu_register(&pmu->pmu, pmu->name, -1);
	if (ret)
		goto release_cpuhp;

	autoread_data->pmu = pmu;

	return 0;

release_cpuhp:
	cpuhp_state_remove_instance(pmu->cpuhp_state, &pmu->cpuhp);
	cpuhp_remove_multi_state(pmu->cpuhp_state);
out_free:
	pr_err("OF: failed to register PMU devices!\n");
	autoread_pmu_free(pmu);
	return ret;
}

static int rtk_autoread_resume(struct device *dev)
{
	int ret;
	struct rtk_autoread_data *autoread_data = dev_get_drvdata(dev);

	ret = autoread_enable(autoread_data);

	return ret;
}

static int rtk_autoread_suspend(struct device *dev)
{
	struct rtk_autoread_data *autoread_data = dev_get_drvdata(dev);

	autoread_disable(autoread_data);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rtk_autoread_pm_ops, rtk_autoread_suspend, rtk_autoread_resume);

static int rtk_autoread_probe(struct platform_device *pdev)
{
	struct rtk_autoread_data *autoread_data;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	enum ina2xx_ids type = ina226;
	int ret = -EINVAL;
	int val;

	autoread_data = devm_kzalloc(dev, sizeof(*autoread_data), GFP_KERNEL);
	if (!autoread_data)
		return -ENOMEM;

	autoread_data->dev = dev;

	kobject_init(&autoread_data->kobj, &rtk_autoread_ktype);
	ret = kobject_add(&autoread_data->kobj, kernel_kobj, "autoread");
	if (ret) {
		dev_err(dev, "[Autoread] Failed to create kobject\n");
		goto err_cleanup;
	}

	ret = sysfs_create_group(&autoread_data->kobj, &attr_group);
	if (ret) {
		dev_err(dev, "[Autoread] Failed to create sysfs group\n");
		goto err_cleanup;
	}

	autoread_data->autoread_addr = of_iomap(np, 0);
	autoread_data->iso_base = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "realtek,iso");
	if (IS_ERR_OR_NULL(autoread_data->iso_base)) {
		dev_err(dev,  "[Autoread] Fail to get iso_base address\n");
		ret = -ENOMEM;
		goto err_cleanup;
	}

	autoread_data->irq = platform_get_irq(pdev, 0);
	if (!autoread_data->irq) {
		dev_err(dev, "[Autoread] Failed to parse autoread IRQ\n");
		ret = -ENXIO;
		goto err_cleanup;
	}

	ret = devm_request_irq(dev, autoread_data->irq, rtk_autoread_irq,
				IRQF_SHARED, dev_name(dev), pdev);
	if (ret) {
		dev_err(dev, "[Autoread] Failed to request IRQ %d (ret=%d)\n",
			autoread_data->irq, ret);
		goto err_cleanup;
	}

	adap = i2c_get_adapter(I2C_BUS_ID);
	if (IS_ERR(adap) || !adap) {
		dev_err(dev, "[Autoread] Get i2c adapter fail\n");
		ret = -EPROBE_DEFER;
		adap = NULL;
		goto err_cleanup;
	}

	if (!of_property_read_u32(pdev->dev.of_node, "ina_type", &val)) {
		switch (val) {
		case 219:
		case 220:
			type = ina219;
			break;
		}
	}
	autoread_data->ina_config = &ina2xx_configs[type];

	platform_set_drvdata(pdev, autoread_data);

	rtk_autoread_ina_setting(autoread_data);

	if (!of_property_read_u32(pdev->dev.of_node, "pmu", &val) && val != 0)
		autoread_pmu_init(autoread_data);

	pm_runtime_enable(autoread_data->dev);
	autoread_enable(autoread_data);

	return 0;

err_cleanup:
	rtk_autoread_cleanup(autoread_data, dev);
	return ret;
}

static int rtk_autoread_remove(struct platform_device *pdev)
{
	struct rtk_autoread_data *autoread_data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	rtk_autoread_cleanup(autoread_data, dev);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id rtk_autoread_match[] = {
	{ .compatible = "realtek,autoread" },
	{},
};

static struct platform_driver rtk_autoread_driver = {
	.probe  = rtk_autoread_probe,
	.remove = rtk_autoread_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "rtk-autoread",
		.pm = &rtk_autoread_pm_ops,
		.of_match_table = of_match_ptr(rtk_autoread_match),
	},
};

static int __init rtk_autoread_init(void)
{
	platform_driver_register(&rtk_autoread_driver);

	return 0;
}
module_init(rtk_autoread_init);

static void __exit rtk_autoread_exit(void)
{
	platform_driver_unregister(&rtk_autoread_driver);
}
module_exit(rtk_autoread_exit);

MODULE_AUTHOR("Adam lin <adam_lin@realtek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek autoread driver");
