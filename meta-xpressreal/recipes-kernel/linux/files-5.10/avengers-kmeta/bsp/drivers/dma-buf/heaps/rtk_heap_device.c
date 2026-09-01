// SPDX-License-Identifier: GPL-2.0-only
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
/*
 * Ioctl definitions
 */

/* Use 'r' as magic number */
#define RHEAP_IOC_MAGIC  'r'
#define RHEAP_GET_BEST_HEAP  _IOR(RHEAP_IOC_MAGIC, 1, struct rheap_best_info)

#define BUF_SIZE	256

struct rheap_best_info {
	u32 flags;
	u8 buf[BUF_SIZE];
};

struct miscdevice *r_mdev = NULL;

static void best_fit_heap_show(u32 flags, char *buf)
{
	LIST_HEAD(best_list);
	struct rtk_best_fit *best_fit;
	bool uncached = (flags & RTK_FLAG_NONCACHED) ? true : false;
	int n = 0;

	if (flags != 0) {
		fill_best_fit_list(NULL, flags, &best_list);
		list_for_each_entry(best_fit, &best_list, hlist) {
			if (uncached)
				n += sprintf(buf+n, "%s_uncached : %u \n",
					 best_fit->name, best_fit->score);
			else
				n += sprintf(buf+n, "%s : %u \n",
					 best_fit->name, best_fit->score);
			if (n > BUF_SIZE)
				BUG();
		}

		for (;;) {
			if (list_empty(&best_list))
				break;
			best_fit = list_last_entry(&best_list,
					 struct rtk_best_fit, hlist);
			list_del(&best_fit->hlist);
			kfree(best_fit);
		}
	}
}



static long rheap_dev_ioctl(struct file *filp, unsigned int cmd,
				 unsigned long arg)
{
	int ret;

	switch (cmd) {
	case RHEAP_GET_BEST_HEAP: {
			LIST_HEAD(best_list);
			struct rheap_best_info best_info;

			if (copy_from_user(&best_info, (void __user *)arg,
					 sizeof(best_info))) {
				pr_err("%s:! RHEAP_GET_BEST_HEAP failed\n",
						 __func__);
				ret = -EFAULT;
				break;
			}

			best_fit_heap_show(best_info.flags, best_info.buf);

			ret = copy_to_user((void __user *)arg, &best_info,
					 sizeof(best_info));
			if (ret) {
				pr_err("%s copy_to_user failed!"
					 "(ret = %d)\n", __func__, ret);
				return -EFAULT;
			}

			break;
		}
	}


	return 0;
}

static const struct file_operations rheap_dev_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = rheap_dev_ioctl,
	.compat_ioctl   = compat_ptr_ioctl,
};

int rheap_miscdev_register(void)
{
	int ret;

	if (r_mdev)
		return 0;

	r_mdev = kzalloc(sizeof(*r_mdev), GFP_KERNEL);
	if (!r_mdev)
		return -ENOMEM;

	r_mdev->minor  = MISC_DYNAMIC_MINOR;
	r_mdev->name   = "rtk_heap";
	r_mdev->fops   = &rheap_dev_fops;
	r_mdev->parent = NULL;

	ret = misc_register(r_mdev);
	if (ret) {
		pr_err("rtk_heap: failed to register misc device.\n");
		kfree(r_mdev);
		return ret;
	}
	return 0;
}

