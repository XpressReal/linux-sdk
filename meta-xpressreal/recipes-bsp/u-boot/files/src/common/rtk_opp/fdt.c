#include <linux/libfdt.h>
#include <rtk_opp.h>
#include <rtk_opp_sysdeps.h>

static int get_prop_version(void *fdt, int offset, const char *name)
{
	char buf[40];
	rtk_opp_snprintf(buf, sizeof(buf), "%s,opp-updated", name);
	return fdt_getprop(fdt, offset, buf, NULL) ? 1 : 2;
}

static const char *conn = " ,-";

int rtk_opp_mark_fdt_updated(void *fdt, const char *name)
{
	int offset;
	char buf[40];
	const void *p;
	int len;
	int v;
	int ret;

	offset = fdt_path_offset(fdt, "/cpu-dvfs");
	if (offset < 0) {
		rtk_opp_printf("%s: invalid fdt path '/cpu-dvfs'\n", __func__);
		return RTK_OPP_ERROR;
	}

	v = get_prop_version(fdt, offset, name);

	rtk_opp_snprintf(buf, sizeof(buf), "%s%copp-updated", name, conn[v]);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (!p || len == 0)
		return RTK_OPP_ERROR;

	ret = fdt_setprop_u32(fdt, offset, buf, 1);
	return ret ? RTK_OPP_ERROR : 0;
}

int rtk_opp_get_fdt_table_offset(void *fdt)
{
	int table_offset;
	table_offset = fdt_path_offset(fdt, "/cpus/cpu");
	if (table_offset > 0) {
		int len;
		const unsigned int *p;

		p = fdt_getprop(fdt, table_offset, "operating-points-v2", &len);
		if (len < 0 || !p)
			table_offset = -FDT_ERR_NOTFOUND;
		else
			table_offset = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(*p));
	}

	if (table_offset < 0)
		table_offset = fdt_path_offset(fdt, "/cpu-dvfs/cpu-opp-table");
	if (table_offset < 0)
		table_offset = fdt_path_offset(fdt, "/cpu-dvfs/opp-table-cpu");
	return table_offset;
}

static int rtk_opp_fdt_getprop_u32_array(void *fdt, int offset, const char *propname,
					 uint32_t voltages[3], uint32_t *n_voltages)
{
	const void *p;
	int len = 0;

	p = fdt_getprop(fdt, offset, propname, &len);
	if (p == NULL || (len != 4 && len != 12))
		return RTK_OPP_ERROR;

	voltages[0] = fdt32_to_cpu(*(const uint32_t *)(p));
	if (len > 4)
		voltages[1] = fdt32_to_cpu(*(const uint32_t *)(p + 4));
	if (len > 8)
		voltages[2] = fdt32_to_cpu(*(const uint32_t *)(p + 8));
	*n_voltages = len / 4;
	return 0;
}

int rtk_opp_update_fdt_table(void *fdt, const char *name, struct rtk_opp_data *data)
{
	int table_offset;
	int offset;
	const void *p;
	int len;
	char buf[40];
	int ret;

	table_offset = rtk_opp_get_fdt_table_offset(fdt);
	if (table_offset < 0) {
		rtk_opp_printf("%s: invalid opp-table path\n", __func__);
		return RTK_OPP_ERROR;
	}

	for (offset = fdt_first_subnode(fdt, table_offset); offset > 0;
	     offset = fdt_next_subnode(fdt, offset)) {
		uint64_t freq_hz;
		uint32_t freq_mhz;
		uint32_t volt_uv;
		uint32_t freq_adj;
		const char *node_name = fdt_get_name(fdt, offset, NULL);
		uint32_t voltages[3] = {}, n_voltages = 1;

		p = fdt_getprop(fdt, offset, "opp-hz", &len);
		if (len < 0) {
			rtk_opp_printf("%s: %s: opp-hz: invalid property\n", __func__, node_name);
			continue;
		}

		freq_hz = fdt64_to_cpu(*(const uint64_t *)p);
		freq_mhz = freq_hz / 1000000;

		/* sepcial case */
		freq_adj = (freq_hz / 1000) % 10;
		if (!freq_adj)
			freq_adj = freq_hz % 10;
		switch (freq_adj) {
		case 1:
			freq_mhz -= 100;
			break;
		case 2:
			freq_mhz -= 75;
			break;
		case 3:
			freq_mhz -= 66;
			break;
		case 4:
			freq_mhz -= 50;
			break;
		case 5:
			freq_mhz -= 33;
			break;
		case 6:
			freq_mhz -= 25;
		default:
			break;
		}

		if (rtk_opp_fdt_getprop_u32_array(fdt, offset, "opp-microvolt", voltages, &n_voltages)) {
			rtk_opp_printf("%s: %s: invalid property 'opp-microvolt'\n", __func__, node_name);
			continue;
		}

		rtk_opp_snprintf(buf, sizeof(buf), "opp-microvolt-%s", name);
		p = fdt_getprop(fdt, offset, buf, &len);
		if (len < 0 || p == NULL) {
			rtk_opp_printf("%s: %s: %s: invalid property\n", __func__, node_name, buf);
			continue;
		}

		volt_uv = rtk_opp_evaluate_voltage(data, freq_mhz);

		if (volt_uv == 0) {
			ret = fdt_setprop_string(fdt, offset, "status", "disabled");
			if (ret)
				ret = fdt_setprop_string(fdt, offset, "status", "disa");
			if (ret)
				ret = fdt_setprop_string(fdt, offset, "status", "di");
			if (ret)
				ret = fdt_setprop(fdt, offset, "status", "", 1);
			if (ret)
				ret = fdt_delprop(fdt, offset, "opp-hz");
			if (ret)
				rtk_opp_printf("%s: %s: failed to set status or del opp-hz\n", __func__, node_name);
			else
				rtk_opp_printf("%s: %s: volt=0, set status or del opp-hz\n", __func__, node_name);
			continue;
		}

		if (volt_uv && n_voltages == 3) {
			fdt32_t v[3];

			v[0] = v[1] = cpu_to_fdt32(volt_uv);
			v[2] = cpu_to_fdt32(voltages[2]);

			ret = fdt_setprop(fdt, offset, buf, v, sizeof(v));

			if (ret)
				rtk_opp_printf("%s: %s: %s: failed to set property\n", __func__, node_name, buf);
			else
				rtk_opp_printf("%s: %s: %s: volt=(%d %d %d), adj=%d\n", __func__, node_name, buf,
					volt_uv, volt_uv, fdt32_to_cpu(v[2]), freq_adj);
		} else {
			ret = fdt_setprop_u32(fdt, offset, buf, volt_uv);

			if (ret)
				rtk_opp_printf("%s: %s: %s: failed to set property\n", __func__, node_name, buf);
			else
				rtk_opp_printf("%s: %s: %s: volt=%d, adj=%d\n", __func__, node_name, buf, volt_uv, freq_adj);
		}
	}

	return 0;
}

int rtk_opp_get_fdt_param(void *fdt, const char *name, struct rtk_opp_param *param)
{
	int offset;
	const void *p;
	int len;
	char buf[40];
	int v;

	offset = fdt_path_offset(fdt, "/cpu-dvfs");
	if (offset < 0) {
		rtk_opp_printf("%s: invalid fdt path '/cpu-dvfs'\n", __func__);
		return RTK_OPP_ERROR;
	}
	v = get_prop_version(fdt, offset, name);

	rtk_opp_snprintf(buf, sizeof(buf), "%s%cvolt-correct", name, conn[v]);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0) {
		int size = len / 4;
		int i;

		if (size > RTK_OPP_MAX_ENTRIES) {
			rtk_opp_printf("%s: size of volt-correct is greater than %d\n", __func__, RTK_OPP_MAX_ENTRIES);
			size = RTK_OPP_MAX_ENTRIES;
		}
		param->num_correct = size;
		for (i = 0; i < size; i++) {
			param->correct[i] = fdt32_to_cpu(*(const uint32_t *)(p + i * 4));
			rtk_opp_printf("%s: volt-correct[%d] = %d\n", __func__, i, param->correct[i]);
		}
	} else {
		return RTK_OPP_ERROR;
	}

	param->step = 37500;
	rtk_opp_snprintf(buf, sizeof(buf), "%s%cvolt-step", name, conn[v]);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0)
		param->step = fdt32_to_cpu(*(const uint32_t *)p);

	param->step_h = 0;
	rtk_opp_snprintf(buf, sizeof(buf), "%s%cvolt-step-high", name, conn[v]);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0)
		param->step_h = fdt32_to_cpu(*(const uint32_t *)p);

	rtk_opp_snprintf(buf, sizeof(buf), "%s%cvolt-min", name, conn[v]);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0)
		param->min = fdt32_to_cpu(*(const uint32_t *)p);

	rtk_opp_snprintf(buf, sizeof(buf), "%s%cvolt-max", name, conn[v]);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0)
		param->max = fdt32_to_cpu(*(const uint32_t *)p);

	param->round = 12500;
	rtk_opp_snprintf(buf, sizeof(buf), "%s%cvolt-round", name, conn[v]);
	p = fdt_getprop(fdt, offset, buf, &len);
	if (p && len > 0)
		param->round = fdt32_to_cpu(*(const uint32_t *)p);

	return 0;
}
