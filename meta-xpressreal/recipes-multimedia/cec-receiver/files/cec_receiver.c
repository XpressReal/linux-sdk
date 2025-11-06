#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include <linux/cec.h>
#include <linux/cec-funcs.h>
#include "cecdefs.h"

#define MY_OSD_NAME  "RTK_AIOT"
#define MY_VENDOR_ID (0x7f80)
#define CEC_DEVICE   "/dev/cec0"

#define PRINT_PHYS_ADDR(pa)         \
    printf("%u.%u.%u.%u (0x%04X)",  \
           ((pa) >> 12) & 0xF,      \
           ((pa) >>  8) & 0xF,      \
           ((pa) >>  4) & 0xF,      \
            (pa)        & 0xF,      \
           (unsigned)(pa))

volatile bool g_running = true;
const char *dev_cec0 = CEC_DEVICE;
uint8_t g_power_status = CEC_OP_POWER_STATUS_ON;
uint8_t g_send_one_touch_play = 0;

void signalHandler(int signum)
{
    if(signum == SIGINT || signum == SIGTERM) {
        printf("Received signal %d, shutting down...\n", signum);
        g_running = false;
    }

    if(signum == SIGUSR1) {
        //CEC_OP_POWER_STATUS_ON (0), CEC_OP_POWER_STATUS_STANDBY (1)
        g_power_status = !g_power_status;
        printf("Toggle power_status to %s\n", (g_power_status == 0 ? "POWER_ON" : "Standby"));
    }

    if(signum == SIGUSR2) {
        printf("Trigger OneTouchPlay\n");
        g_send_one_touch_play = 1;
    }
}

static const char *la_type2str(unsigned int t)
{
	switch (t) {
	case CEC_LOG_ADDR_TYPE_TV:          return "TV";
	case CEC_LOG_ADDR_TYPE_RECORD:      return "Record";
	case CEC_LOG_ADDR_TYPE_TUNER:       return "Tuner";
	case CEC_LOG_ADDR_TYPE_PLAYBACK:    return "Playback";
	case CEC_LOG_ADDR_TYPE_AUDIOSYSTEM: return "Audio System";
	case CEC_LOG_ADDR_TYPE_SPECIFIC:    return "Specific";
	case CEC_LOG_ADDR_TYPE_UNREGISTERED:return "Unregistered";
	default:                            return "Unknown";
	}
}

static const char *la_2str(unsigned int t)
{
    switch (t) {
    case CEC_LOG_ADDR_TV:           return "TV";
    case CEC_LOG_ADDR_RECORD_1:     return "Record 1";
    case CEC_LOG_ADDR_RECORD_2:     return "Record 2";
    case CEC_LOG_ADDR_TUNER_1:      return "Tuner 1";
    case CEC_LOG_ADDR_PLAYBACK_1:   return "Playback 1";
    case CEC_LOG_ADDR_AUDIOSYSTEM:  return "Audio";
    case CEC_LOG_ADDR_TUNER_2:      return "Tuner 2";
    case CEC_LOG_ADDR_TUNER_3:      return "Tuner 3";
    case CEC_LOG_ADDR_PLAYBACK_2:   return "Playback 3";
    case CEC_LOG_ADDR_RECORD_3:     return "Record 3";
    case CEC_LOG_ADDR_TUNER_4:      return "Tuner 4";
    case CEC_LOG_ADDR_PLAYBACK_3:   return "Playback 3";
    case CEC_LOG_ADDR_BACKUP_1:     return "Backup 1";
    case CEC_LOG_ADDR_BACKUP_2:     return "Backup 2";
    case CEC_LOG_ADDR_SPECIFIC:     return "Specific";
    case CEC_LOG_ADDR_BROADCAST:    return "Broadcast";
    default:                        return "Unknown";
    }
}
static const char *prim_dev2str(unsigned int d)
{
    switch (d) {
    case CEC_OP_PRIM_DEVTYPE_TV:            return "TV";
    case CEC_OP_PRIM_DEVTYPE_RECORD:        return "Record";
    case CEC_OP_PRIM_DEVTYPE_TUNER:         return "Tuner";
    case CEC_OP_PRIM_DEVTYPE_PLAYBACK:      return "Playback";
    case CEC_OP_PRIM_DEVTYPE_AUDIOSYSTEM:   return "Audio";
    case CEC_OP_PRIM_DEVTYPE_SWITCH:        return "Switch";
    case CEC_OP_PRIM_DEVTYPE_PROCESSOR:     return "Processor";
    default:                                return "Unknown";
    }
}
/* ----------------------------------------------------------------*/

void print_cec_infomation(const cec_adapter_t *adapter)
{
    struct cec_caps *caps = adapter->caps;
    struct cec_log_addrs *la = adapter->log_addrs;
    int i;

    printf("Configured as Playback Device\n");
    printf("CEC adapter capabilities for %s\n", adapter->device);
    printf("  driver:        %s\n", caps->driver);
    printf("  name:          %s\n", caps->name);
    printf("  capabilities:  0x%08x\n", caps->capabilities);
    printf("  version:       %u.%u.%u\n",
           (caps->version >> 16) & 0xff,
           (caps->version >> 8)  & 0xff,
           caps->version & 0xff);
    printf("  available LA:  %u\n", caps->available_log_addrs);
    printf("========== struct cec_log_addrs ==========\n");

    printf("log_addr_mask        : 0x%04x\n", la->log_addr_mask);
    printf("log_addrs occupied   : %u\n",     la->num_log_addrs);
    printf("CEC version          : 0x%02x (%s)\n",
           la->cec_version,
           la->cec_version == CEC_OP_CEC_VERSION_1_3A ? "1.3a" :
           la->cec_version == CEC_OP_CEC_VERSION_1_4    ? "1.4"   :
           la->cec_version == CEC_OP_CEC_VERSION_2_0    ? "2.0"   : "?");

    printf("num_log_addrs        : 0x%04x\n", la->num_log_addrs);
    for (i = 0; i < la->num_log_addrs; ++i) {
        printf("  [ Logical Addr %02d ] --------\n", i);
        printf("  logical address : 0x%02x\n",    la->log_addr[i]);
        printf("  logical type    : %s\n",        la_type2str(la->log_addr_type[i]));
        printf("  primary type    : %s\n",        prim_dev2str(la->primary_device_type[i]));
        printf("  broadcast type  : %s\n",
               la->all_device_types[i] == CEC_OP_ALL_DEVTYPE_TUNER ? "Tuner"
             : la->all_device_types[i] == CEC_OP_ALL_DEVTYPE_RECORD ? "Record"
             : la->all_device_types[i] == CEC_OP_ALL_DEVTYPE_PLAYBACK ? "Playback"
             : la->all_device_types[i] == CEC_OP_ALL_DEVTYPE_AUDIOSYSTEM ? "Audio"
             : "Switch");
        printf("  osd name        : \"%s\"\n",    la->osd_name);
        printf("  -----------------------------------\n");
    }

    printf("==========================================\n");
}

int sleep_ms(int milliseconds)
{
    if(milliseconds < 0) {
        return -1;
    }

    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;

    return nanosleep(&ts, NULL);
}

int cec_transmit_msg(cec_adapter_t *adapter, struct cec_msg *reply)
{
    int fd = adapter->fd;
    int ret = 0;

    for(int i=0; i<5; i++) {
        ret = ioctl(fd, CEC_TRANSMIT, reply);
        //printf("tx_status: %d tx_error_cnt: %d\n", msg->tx_status, msg->tx_error_cnt);
        if(ret == 0)
            break;
        if(errno == EBUSY) {
            sleep_ms(10);
            continue;
        }
    }
//    if(ret < 0)
//        fprintf(stderr, "[%s](%d) ioctl failed: %s\n", __func__, __LINE__, strerror(errno));

    return ret;
}

void update_physicalAddr(cec_adapter_t *adapter)
{
    if (ioctl(adapter->fd, CEC_ADAP_G_PHYS_ADDR, &adapter->physicalAddr) < 0) {
        fprintf(stderr, "[%s](%d) ioctl failed: %s\n", __func__, __LINE__, strerror(errno));
    }
}
void update_logicalAddr(cec_adapter_t *adapter)
{
    if (ioctl(adapter->fd, CEC_ADAP_G_LOG_ADDRS, adapter->log_addrs) < 0) {
        fprintf(stderr, "[%s](%d) ioctl failed: %s\n", __func__, __LINE__, strerror(errno));
    } else {
        adapter->logicalAddr = adapter->log_addrs->log_addr[0];
    }
}

void update_cec_caps(cec_adapter_t *adapter)
{
    struct cec_caps *caps;

    caps = (struct cec_caps *)malloc(sizeof(struct cec_caps));
    memset(caps, 0, sizeof(struct cec_caps));

    if (ioctl(adapter->fd, CEC_ADAP_G_CAPS, caps) < 0) {
        fprintf(stderr, "[%s](%d) ioctl failed: %s\n", __func__, __LINE__, strerror(errno));
    } else {
        adapter->caps = caps;
    }
}

void send_physical_addresss(cec_adapter_t *adapter)
{
    struct cec_msg msg = {0};
    uint8_t prim_devtype = adapter->prim_devtype;
    cec_msg_init(&msg, adapter->logicalAddr, 0xf);
    cec_msg_report_physical_addr(&msg, adapter->physicalAddr, prim_devtype);
    cec_transmit_msg(adapter, &msg);
}

void send_device_vendor_id(cec_adapter_t *adapter)
{
    struct cec_msg msg = {0};
    uint32_t vendor_id = adapter->log_addrs->vendor_id;
    cec_msg_init(&msg, adapter->logicalAddr, 0xf);
    cec_msg_device_vendor_id(&msg, vendor_id);
    cec_transmit_msg(adapter, &msg);
}

void send_one_touch_play(cec_adapter_t *adapter)
{
    struct cec_msg msg = {0};

    send_physical_addresss(adapter);
    send_device_vendor_id(adapter);

    //image view on
    cec_msg_init(&msg, adapter->logicalAddr, 0x0);
    cec_msg_image_view_on(&msg);
    cec_transmit_msg(adapter, &msg);
    printf("Send Image View On\n");

    //active source
    cec_msg_init(&msg, adapter->logicalAddr, 0xf);
    cec_msg_active_source(&msg, adapter->physicalAddr);
    cec_transmit_msg(adapter, &msg);
    printf("Send Active Source\n");
}

void cec_user_keypress(void* cbparam, const cec_keypress* key)
{
    (void)cbparam;
    if(!key)
        return;

    if(key->duration != 0) {
        switch (key->keycode)
        {
           case CEC_USER_CONTROL_CODE_SELECT:
                printf("  -> SELECT key pressed!\n");
                break;
           case CEC_USER_CONTROL_CODE_UP:
                printf("  -> UP key pressed!\n");
                break;
            case CEC_USER_CONTROL_CODE_DOWN:
                printf("  -> DOWN key pressed!\n");
                break;
            case CEC_USER_CONTROL_CODE_LEFT:
                printf("  -> LEFT key pressed!\n");
                break;
            case CEC_USER_CONTROL_CODE_RIGHT:
                printf("  -> RIGHT key pressed!\n");
                break;
            case CEC_USER_CONTROL_CODE_EXIT:
                printf("  -> Back key pressed!\n");
                break;
            default:
                printf("  -> Other key pressed with code: %d\n", (int)key->keycode);
                break;
        }
    }
}

void cec_recvd_command(void *cbparam, void *cmd)
{
    (void)cbparam;
    cec_adapter_t *adapter = (cec_adapter_t *)cmd;
    struct cec_msg *recv_msg = adapter->msg;
    struct cec_msg reply;

	uint8_t src_addr = cec_msg_destination(recv_msg);
	uint8_t dst_addr = cec_msg_initiator(recv_msg);

    int opcode = cec_msg_opcode(recv_msg);

    uint16_t physicalAddr = adapter->physicalAddr;
    uint8_t log_addr = adapter->log_addrs->log_addr[0];

    cec_msg_init(&reply, src_addr, dst_addr);
    reply.timeout = 1000;
    sleep_ms(10);

    switch(opcode) {
        case CEC_MSG_ROUTING_CHANGE: { //0x80
            if(recv_msg->len < 6)
                break;
            adapter->physicalAddr = (recv_msg->msg[4] << 8) | recv_msg->msg[5];
            printf("Routing Change Port:");
            PRINT_PHYS_ADDR(adapter->physicalAddr);
            printf("\n");
            break;
        }
        case CEC_MSG_GIVE_PHYSICAL_ADDR: { //0x83
            send_physical_addresss(adapter);
            printf("Give Physical address %d\n", physicalAddr);
            break;
        }
        case CEC_MSG_REQUEST_ACTIVE_SOURCE: { //0x85
            cec_msg_init(&reply, log_addr , 0xf);
            cec_msg_active_source(&reply, physicalAddr);
            cec_transmit_msg(adapter, &reply);
            printf("Reply Active Source \n");
            break;
        }
        case CEC_MSG_GIVE_DEVICE_VENDOR_ID: { //0x8c
            send_device_vendor_id(adapter);
            printf("Give Device Vendor ID: %d\n", adapter->log_addrs->vendor_id);
            break;
        }
        case CEC_MSG_GIVE_DEVICE_POWER_STATUS: { //0x8f
            cec_msg_report_power_status(&reply, g_power_status);
            cec_transmit_msg(adapter, &reply);
            printf("Report Device Power Status %d\n", g_power_status);
            break;
        }
        case CEC_MSG_GIVE_OSD_NAME: { //0x46
            cec_msg_set_osd_name(&reply, adapter->log_addrs->osd_name);
            cec_transmit_msg(adapter, &reply);
            printf("Give OSD Name: %s\n", adapter->log_addrs->osd_name);
            break;
        }
        default:
            //printf("opcode=0x%02x\n", opcode);
            break;
    }
}

static void *cec_monitor_thread(void *arg)
{
    cec_adapter_t *adapter = (cec_adapter_t *)arg;
    cec_callback_t *callbacks = adapter->callbacks;
    fd_set rfds;
    struct timeval tv;
    struct cec_msg *msg;
    uint8_t  prev_key = 0xff;
    uint64_t press_ts = 0;
    int fd = adapter->fd;

    printf("[] %s(%d)\n", __func__, __LINE__);

    msg = (struct cec_msg *)malloc(sizeof(struct cec_msg));
    uint32_t mode = (CEC_MODE_INITIATOR | CEC_MODE_FOLLOWER);
    //uint32_t mode = CEC_MODE_MONITOR;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    if (ioctl(fd, CEC_S_MODE, &mode) < 0) {
        fprintf(stderr, "[%s](%d) ioctl failed: %s\n", __func__, __LINE__, strerror(errno));
    }

    int hpd_sock = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
    if (hpd_sock < 0) {
        perror("socket");
    }

    struct sockaddr_nl addr = { .nl_family = AF_NETLINK,
                                .nl_groups = 1 };
    if (bind(hpd_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(hpd_sock);
    }

    int maxfd = (hpd_sock > fd ? hpd_sock : fd);
    int prev_hpd, curr_hpd;

    while (adapter->monitor) {
        int r;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        FD_SET(hpd_sock, &rfds);
        fflush(stdout);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if(g_send_one_touch_play) {
            send_one_touch_play(adapter);
            g_send_one_touch_play = 0;
        }

        r = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (r < 0)
            break;

        if (FD_ISSET(fd, &rfds)) {
            memset(msg, 0, sizeof(struct cec_msg));

            r = ioctl(fd, CEC_RECEIVE, msg);
			if (r == ENODEV) {
				fprintf(stderr, "Device was disconnected.\n");
				break;
			}
			if (r) {
                sleep_ms(10);
				continue;
            }

			uint8_t from = cec_msg_initiator(msg);
			uint8_t to = cec_msg_destination(msg);
            uint8_t transmit = msg->tx_status;
            const char *direction = (transmit != 0) ? "Transmitted by" : "Received from";
            const char *from_str = la_2str(from&0xf);
            const char *to_str = (to == 0xf) ? "All" : la_2str(to&0xf);

			printf("%s %s to %s (%d to %d): opcode=0x%02X\n",
                    direction, from_str, to_str,
                    from, to, msg->msg[1]);

            switch (msg->msg[1]) {
                case CEC_MSG_USER_CONTROL_PRESSED: {
                    if (msg->len < 3) break;

                    prev_key = msg->msg[2];
                    press_ts = msg->rx_ts;
                    break;
                }
                case CEC_MSG_USER_CONTROL_RELEASED: {
                    if (prev_key == 0xff) break;

                    uint64_t now = msg->rx_ts;
                    uint64_t ms = (now - press_ts) / 1000000UL;
                    cec_keypress key;
                    key.keycode = prev_key;
                    key.duration = ms;
                    callbacks->key_press(NULL, &key);

                    prev_key = 0xff;
                    break;
                }
                default: {
                    adapter->msg = msg;
                    callbacks->cmd_received(NULL, adapter);
                    break;
                }
            }
        }

        if(FD_ISSET(hpd_sock, &rfds)) {
            char buf[4096];
            int len;
            len = recv(hpd_sock, buf, sizeof(buf), 0);
            if (len > 0 &&
                memmem(buf, len, "ACTION=change", 13) &&
                memmem(buf, len, "DEVPATH=", 8)       &&
                memmem(buf, len, "OF_NAME=hdmi", 12)) {

                printf("--- HDMI HPD uevent ---\n");
                if(memmem(buf, len, "HDMI_HPD=1", 10))
                    curr_hpd = 1;
                if(memmem(buf, len, "HDMI_HPD=0", 10))
                    curr_hpd = 0;
                if(prev_hpd == 0 && curr_hpd == 1) {
                    printf("HDMI cable plugin\n");
                    update_physicalAddr(adapter);
                    update_logicalAddr(adapter);
                    send_one_touch_play(adapter);
                }
                //for (char *p = buf; p < buf + len; p += strlen(p) + 1) {
                //    puts(p);
                //}

                prev_hpd = curr_hpd;
            }
        }
    }

    mode = CEC_MODE_NO_INITIATOR | CEC_MODE_NO_FOLLOWER;
    if(ioctl(fd, CEC_S_MODE, &mode) < 0) {
        fprintf(stderr, "[%s](%d) ioctl failed: %s\n", __func__, __LINE__, strerror(errno));
    }
    free(msg);
    msg = NULL;

    fprintf(stderr, "[] %s(%d)\n", __func__, __LINE__);

    close(hpd_sock);
    return NULL;
}

int cec_init(cec_adapter_t *adapter, int deviceType, const char* osd_name)
{
    struct cec_log_addrs *log_addrs;
    log_addrs = (struct cec_log_addrs *)malloc(sizeof(struct cec_log_addrs));
    memset(log_addrs, 0, sizeof(struct cec_log_addrs));

    adapter->fd = open(adapter->device, O_RDWR | O_NONBLOCK);
    if(adapter->fd < 0) {
	    fprintf(stderr, "Failed to open %s: %s\n", adapter->device, strerror(errno));
        return -1;
    }

    if (ioctl(adapter->fd, CEC_ADAP_S_LOG_ADDRS, log_addrs) < 0) {
        fprintf(stderr, "[%s](%d) ioctl failed: %s\n", __func__, __LINE__, strerror(errno));
    }

    log_addrs->cec_version = CEC_VERSION_1_4;
    log_addrs->num_log_addrs = 1;

    if(deviceType == CEC_OP_PRIM_DEVTYPE_PLAYBACK) {
        log_addrs->log_addr_type[0]      = CEC_LOG_ADDR_TYPE_PLAYBACK;
        log_addrs->primary_device_type[0] = CEC_OP_PRIM_DEVTYPE_PLAYBACK;
        log_addrs->all_device_types[0]    = CEC_OP_ALL_DEVTYPE_PLAYBACK;
    }
    log_addrs->flags = CEC_LOG_ADDRS_FL_ALLOW_RC_PASSTHRU;
    log_addrs->vendor_id = MY_VENDOR_ID;
    strcpy(log_addrs->osd_name, osd_name);

    if (ioctl(adapter->fd, CEC_ADAP_S_LOG_ADDRS, log_addrs) < 0) {
        fprintf(stderr, "[%s](%d) ioctl failed: %s\n", __func__, __LINE__, strerror(errno));
    }

    adapter->log_addrs = log_addrs;

    sleep_ms(500);

    update_physicalAddr(adapter);
    update_logicalAddr(adapter);
    update_cec_caps(adapter);

    return 0;
}

int cec_create_monitor(cec_adapter_t *adapter)
{
    printf("[] %s\n", __func__);

    adapter->monitor = true;
    print_cec_infomation(adapter);
    pthread_create(&adapter->cec_tid, NULL, cec_monitor_thread, (void *)adapter);

    return 0;
}

int cec_close(cec_adapter_t *adapter)
{
    printf("[] %s(%d)\n", __func__, __LINE__);

    adapter->monitor = false;
    pthread_join(adapter->cec_tid, NULL);

    close(adapter->fd);
    free(adapter->log_addrs);
    free(adapter->caps);
    free(adapter);

    printf("[] %s(%d)\n", __func__, __LINE__);
    return 0;
}

int main()
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGUSR1, signalHandler);
    signal(SIGUSR2, signalHandler);

    const char *my_osd_name = MY_OSD_NAME;

    cec_callback_t callbacks;
    callbacks.cmd_received = &cec_recvd_command;
    callbacks.key_press = &cec_user_keypress;

    cec_adapter_t *cec_adapter;
    cec_adapter = (cec_adapter_t *)malloc(sizeof(cec_adapter_t));
    memset(cec_adapter, 0, sizeof(cec_adapter_t));
    cec_adapter->device = dev_cec0;
    cec_adapter->callbacks = &callbacks;

    pid_t my_pid = getpid();
    printf("My PID is: %d\n", my_pid);

    cec_init(cec_adapter, CEC_OP_PRIM_DEVTYPE_PLAYBACK, my_osd_name);
    cec_create_monitor(cec_adapter);

    while(g_running) {
        sleep_ms(1000);
    }

    cec_close(cec_adapter);

    return 0;
}
