#include <linux/io.h>
#include <soc/realtek/kernel-rpc.h>
#include <rtk_rpc.h>
#include "snd-realtek_common.h"
#include "common.h"

#if IS_ENABLED(CONFIG_SND_HIFI_AO)
#include "snd-hifi-realtek.h"

static inline u32 cpu_to_remote32(u32 val)
{
        return val;
}

static inline u32 remote_align(u32 size)
{
        return ALIGN(get_rpc_alignment_offset(size), 128);
}

#define remote_ept_info hifi_ept_info
#define REMOTE_OPT  RPC_HIFI

#else
#include "snd-realtek.h"

static inline u32 cpu_to_remote32(u32 val)
{
        return htonl(val);
}

static inline u32 remote_align(u32 size)
{
        return get_rpc_alignment_offset(size);
}

#define REMOTE_OPT  RPC_AUDIO

#endif

int rtk_aio_ctrl_rpc_setup(struct device *dev, struct rtk_aio_ctrl_rpc *rpc)
{
	return 0;
}

int rtk_aio_ctrl_rpc_send_msg(struct rtk_aio_ctrl_rpc *rpc, u32 command, dma_addr_t addr, u32 size)
{
	u32 rpc_ret;
	u32 off = remote_align(size);

	if (send_rpc(REMOTE_OPT, command, addr, addr + off, &rpc_ret))
		return -EINVAL;

	return rpc_ret != S_OK ? -EINVAL : 0;
}

u32 rtk_aio_ctrl_rpc_to_remote32(struct rtk_aio_ctrl_rpc *rpc, u32 val)
{
	return cpu_to_remote32(val);
}
