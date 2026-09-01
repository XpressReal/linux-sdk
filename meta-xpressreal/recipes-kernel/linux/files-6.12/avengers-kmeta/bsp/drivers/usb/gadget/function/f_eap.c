//#define DEBUG

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/configfs.h>
#include <linux/usb/composite.h>

#include "configfs.h"

#define EAP_BULK_BUFFER_SIZE           4096

/* number of tx requests to allocate */
#define TX_REQ_MAX 4

#define EAP_GET_DEVICE_STATE               _IOR('z', 1, int)

static const char eap_shortname[] = "eap";

struct eap_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	int online;
	int error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req;
	int rx_done;

	struct work_struct work;
	struct miscdevice *misc_device;
	int sw_online;
};

static struct usb_interface_assoc_descriptor eap_iad_desc = {
	.bLength =		sizeof eap_iad_desc,
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,

	/* .bFirstInterface =	DYNAMIC, */
	.bInterfaceCount =	1, /* data */
	.bFunctionClass =	0x02,
	.bFunctionSubClass =	0x0D,
	.bFunctionProtocol =	0x0,
	/* .iFunction =		DYNAMIC */
};

static struct usb_interface_descriptor eap_interface_desc0 = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 1,
	.bAlternateSetting	= 0,
	.bNumEndpoints          = 0,
	.bInterfaceClass        = 0xFF,
	.bInterfaceSubClass     = 0xF0,
	.bInterfaceProtocol     = 1,
};

static struct usb_interface_descriptor eap_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 1,
	.bAlternateSetting	= 1,
	.bNumEndpoints          = 2,
	.bInterfaceClass        = 0xFF,
	.bInterfaceSubClass     = 0xF0,
	.bInterfaceProtocol     = 1,
};

static struct usb_endpoint_descriptor eap_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor eap_highspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor eap_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor eap_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_eap_descs[] = {
	(struct usb_descriptor_header *) &eap_iad_desc,
	(struct usb_descriptor_header *) &eap_interface_desc0,
	(struct usb_descriptor_header *) &eap_interface_desc,
	(struct usb_descriptor_header *) &eap_fullspeed_in_desc,
	(struct usb_descriptor_header *) &eap_fullspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *hs_eap_descs[] = {
	(struct usb_descriptor_header *) &eap_iad_desc,
	(struct usb_descriptor_header *) &eap_interface_desc0,
	(struct usb_descriptor_header *) &eap_interface_desc,
	(struct usb_descriptor_header *) &eap_highspeed_in_desc,
	(struct usb_descriptor_header *) &eap_highspeed_out_desc,
	NULL,
};

static struct usb_string eap_string_defs[] = {
	[0].s = "com.baidu.CarLifeVehicleProtocol",
	{  } /* end of list */
};

static struct usb_gadget_strings eap_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		eap_string_defs,
};

static struct usb_gadget_strings *eap_strings[] = {
	&eap_string_table,
	NULL,
};

#define DRIVER_NAME "eap"
#define MAX_INST_NAME_LEN          40

struct eap_instance {
	struct usb_function_instance func_inst;
	const char *name;
	struct eap_dev *dev;
	char eap_ext_compat_id[16];
	struct usb_os_desc eap_os_desc;
};

/* temporary variable used between eap_open() and eap_gadget_bind() */
static struct eap_dev *_eap_dev;

static inline struct eap_dev *func_to_eap(struct usb_function *f)
{
	return container_of(f, struct eap_dev, function);
}

static struct usb_request *eap_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void eap_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int eap_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void eap_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

/* add a request to the tail of a list */
void eap_req_put(struct eap_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
struct usb_request *eap_req_get(struct eap_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	pr_debug("Enter %s %d\n", __func__, __LINE__);
	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void eap_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct eap_dev *dev = _eap_dev;

	pr_debug("Enter %s %d\n", __func__, __LINE__);
	if (req->status != 0)
		dev->error = 1;
	pr_debug("%s Check status=%d error=%d\n", __func__, req->status, dev->error);
	eap_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void eap_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct eap_dev *dev = _eap_dev;

	pr_debug("%s %d\n", __func__, __LINE__);
	dev->rx_done = 1;
	//if (req->status != 0)
	//	dev->error = 1;
	pr_debug("%s Check rx_done=%d status=%d error=%d\n", __func__,
		 dev->rx_done, req->status, dev->error);
	wake_up(&dev->read_wq);
}

static int eap_create_bulk_endpoints(struct eap_dev *dev,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	DBG(cdev, "create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for eap ep_out got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	req = eap_request_new(dev->ep_out, EAP_BULK_BUFFER_SIZE);
	if (!req)
		goto fail;
	req->complete = eap_complete_out;
	dev->rx_req = req;

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = eap_request_new(dev->ep_in, EAP_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = eap_complete_in;
		eap_req_put(dev, &dev->tx_idle, req);
	}

	return 0;

fail:
	printk(KERN_ERR "eap_bind() could not allocate requests\n");
	return -1;
}

static ssize_t eap_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	struct eap_dev *dev = fp->private_data;
	struct usb_request *req;
	int r = count, xfer;
	int ret;

	pr_debug("eap_read(%d)\n", count);
	if (!_eap_dev)
		return -ENODEV;

	if (count > EAP_BULK_BUFFER_SIZE)
		return -EINVAL;

	if (eap_lock(&dev->read_excl))
		return -EBUSY;

	pr_debug("%s %d online=%d error=%d\n", __func__, __LINE__, dev->online, dev->error);
	/* we will block until we're online */
	while (!(dev->online || dev->error)) {
		pr_debug("eap_read: waiting for online state\n");
		ret = wait_event_interruptible(dev->read_wq,
				(dev->online || dev->error));
		if (ret <= 0) {
			eap_unlock(&dev->read_excl);
			return ret;
		}
	}
	if (dev->error) {
		r = -EIO;
		goto done;
	}

requeue_req:
	/* queue a request */
	req = dev->rx_req;
	req->length = count;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (ret < 0) {
		pr_debug("eap_read: failed to queue req %p (%d)\n", req, ret);
		r = -EIO;
		dev->error = 1;
		goto done;
	} else {
		pr_debug("rx %p queue\n", req);
	}

	/* wait for a request to complete */
	//ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	ret = wait_event_interruptible_timeout(dev->read_wq, dev->rx_done, msecs_to_jiffies(1000));
	if (ret < 0) {
		dev->error = 1;
		r = ret;
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	} else if (ret == 0) {
			r = 0;
			usb_ep_dequeue(dev->ep_out, req);
			goto done;
	}

	if (!dev->error) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		pr_debug("rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;
		pr_debug("EAP: usb_ep_queue, len=%d, data:r", xfer);
		//print_hex_dump(KERN_INFO, "EAP RX: ", DUMP_PREFIX_OFFSET, 16, 1,
		//			                       req->buf, xfer, false);
		r = xfer;
	} else
		r = -EIO;
	//pr_debug("%s %d\n", __func__, __LINE__);
done:
	eap_unlock(&dev->read_excl);
	return r;
}

static ssize_t eap_write(struct file *fp, const char __user *buf,
				 size_t count, loff_t *pos)
{
	struct eap_dev *dev = fp->private_data;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;
//	pr_debug("%s %d\n", __func__, __LINE__);
	if (!_eap_dev)
		return -ENODEV;

	if (eap_lock(&dev->write_excl))
		return -EBUSY;

	/* we will block until we're online */
	while (!(dev->online || dev->error)) {
		pr_debug("eap_read: waiting for online state\n");
		ret = wait_event_interruptible(dev->write_wq,
				(dev->online || dev->error));
		if (ret <= 0) {
			eap_unlock(&dev->write_excl);
			return ret;
		}
	}
	if (dev->error) {
		r = -EIO;
		goto done;
	}

	while (count > 0) {
		if (dev->error) {
			printk("eap_write dev->error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			(req = eap_req_get(dev, &dev->tx_idle)) || dev->error);
//		ret = wait_event_interruptible_timeout(dev->write_wq,
//			(req = eap_req_get(dev, &dev->tx_idle)) || dev->error,  HZ*5);

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > EAP_BULK_BUFFER_SIZE)
				xfer = EAP_BULK_BUFFER_SIZE;
			else
				xfer = count;
			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}
			pr_debug("EAP: usb_ep_queue, len=%d, data:w", xfer);
		//	print_hex_dump(KERN_INFO, "EAP TX: ", DUMP_PREFIX_OFFSET, 16, 1,
		//			                       req->buf, xfer, false);
			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				dev->error = 1;
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req)
		eap_req_put(dev, &dev->tx_idle, req);
done:
	eap_unlock(&dev->write_excl);
	return r;
}

static int eap_open(struct inode *ip, struct file *fp)
{
	pr_debug("%s %d\n", __func__, __LINE__);
	if (!_eap_dev)
		return -ENODEV;

	if (eap_lock(&_eap_dev->open_excl))
		return -EBUSY;

	fp->private_data = _eap_dev;

	/* clear the error latch */
	_eap_dev->error = 0;

	return 0;
}

static int eap_release(struct inode *ip, struct file *fp)
{
	eap_unlock(&_eap_dev->open_excl);
	return 0;
}

static long eap_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct eap_dev *dev = file->private_data;
	int online;

	switch (cmd){
		case EAP_GET_DEVICE_STATE:
			if(dev->online)
				online = 1;
			else
				online = 0;
		}

	if (copy_to_user((void __user *)arg, &online, sizeof(online)))
		return -1;

	printk("eap_ioctl online=%d >>>>>>>>>>>>>>>>>>>>>>>>>>\n", online);
	return 0;
}


static struct file_operations eap_fops = {
	.owner = THIS_MODULE,
	.read = eap_read,
	.write = eap_write,
	.open = eap_open,
	.unlocked_ioctl = eap_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = eap_ioctl,
#endif
	.release = eap_release,
};

static struct miscdevice eap_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = eap_shortname,
	.fops = &eap_fops,
};


static void eap_work(struct work_struct *data)
{
	struct eap_dev *dev = container_of(data, struct eap_dev, work);
	char *disconnected[2] = { "EAP_STATE=DISCONNECTED", NULL };
	char *connected[2]    = { "EAP_STATE=CONNECTED", NULL };
	char **uevent_envp = NULL;

	pr_debug("%s %d\n", __func__, __LINE__);
	if(dev->online != dev->sw_online){
			if(dev->online)
				uevent_envp = connected;
			else
				uevent_envp = disconnected;
		}
		dev->sw_online = dev->online;

	if (uevent_envp) {
		kobject_uevent_env(&dev->misc_device->this_device->kobj, KOBJ_CHANGE, uevent_envp);
		pr_info("%s: sent uevent %s\n", __func__, uevent_envp[0]);
	} else {
		pr_info("%s: did not send uevent (%d %p)\n", __func__,
		dev->sw_online, uevent_envp);
	}
}

static int eap_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct eap_dev	*dev = func_to_eap(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	printk("eap_function_set_alt alt=%d\n", alt);

	if (alt == 0)
		return 0;

	if (config_ep_by_speed(cdev->gadget, f,
				   dev->ep_in) ||
		config_ep_by_speed(cdev->gadget, f,
				   dev->ep_out)) {
		pr_debug("%s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	usb_ep_enable(dev->ep_in);
	usb_ep_enable(dev->ep_out);

	dev->online = 1;
	printk("eap_function_set_alt: online\n");

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	wake_up(&dev->write_wq);

	pr_debug("%s %d schedule_work\n", __func__, __LINE__);
	schedule_work(&dev->work);

	return 0;
}

static int
eap_function_get_alt(struct usb_function *f, unsigned interface)
{
	//struct eap_dev	*dev = func_to_eap(f);

	printk("eap_function_get_alt\n");

	return 0;
}

static int
eap_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct eap_dev	*dev = func_to_eap(f);
	int			id;
	int			ret;
	int 		status;

	printk("eap_function_bind\n");

	dev->cdev = cdev;
	DBG(cdev, "eap_function_bind dev: %p\n", dev);

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	eap_interface_desc0.bInterfaceNumber = id;
	eap_interface_desc.bInterfaceNumber = id;
	eap_iad_desc.iFunction = id;
	eap_iad_desc.bFirstInterface = id;

	status = usb_string_id(cdev);
	if (status < 0)
		return status;
	eap_string_defs[0].id = status;
	eap_interface_desc0.iInterface = status;
	eap_interface_desc.iInterface = status;

	/* copy descriptors, and track endpoint copies */
	dev->function.fs_descriptors = usb_copy_descriptors(fs_eap_descs);

	/* allocate endpoints */
	ret = eap_create_bulk_endpoints(dev, &eap_fullspeed_in_desc,
			&eap_fullspeed_out_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		eap_highspeed_in_desc.bEndpointAddress =
			eap_fullspeed_in_desc.bEndpointAddress;
		eap_highspeed_out_desc.bEndpointAddress =
			eap_fullspeed_out_desc.bEndpointAddress;

		dev->function.hs_descriptors = usb_copy_descriptors(hs_eap_descs);
	}

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void
eap_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct eap_dev	*dev = func_to_eap(f);
	struct usb_request *req;

	printk("eap_function_unbind\n");

	dev->online = 0;
	dev->error = 1;

	wake_up(&dev->read_wq);
	wake_up(&dev->write_wq);

	pr_debug("%s %d\n", __func__, __LINE__);
	schedule_work(&dev->work);

	eap_request_free(dev->rx_req, dev->ep_out);
	while ((req = eap_req_get(dev, &dev->tx_idle)))
		eap_request_free(req, dev->ep_in);
}

static void eap_function_disable(struct usb_function *f)
{
	struct eap_dev	*dev = func_to_eap(f);
	printk("eap_function_disable\n");

	dev->online = 0;
	dev->error = 1;
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	wake_up(&dev->write_wq);

	pr_debug("%s %d\n", __func__, __LINE__);
	schedule_work(&dev->work);
}

static int __eap_setup(struct eap_instance *fi_eap)
{
	struct eap_dev *dev;
	int ret;

	pr_debug("%s %d\n", __func__, __LINE__);
	dev = kzalloc(sizeof(struct eap_dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	if (fi_eap != NULL)
		fi_eap->dev = dev;

	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);

	INIT_LIST_HEAD(&dev->tx_idle);
	INIT_WORK(&dev->work, eap_work);

	dev->misc_device = &eap_device;
	_eap_dev = dev;

	ret = misc_register(&eap_device);
	if (ret)
		goto err;

	return 0;

err:
	kfree(dev);
	printk(KERN_ERR "eap gadget driver failed to initialize\n");
	return ret;
}

static int eap_setup_configfs(struct eap_instance *fi_eap)
{
	pr_debug("%s %d\n", __func__, __LINE__);
	return __eap_setup(fi_eap);
}

static void eap_cleanup(void)
{
	struct eap_dev *dev = _eap_dev;

	pr_debug("%s %d\n", __func__, __LINE__);
	if (!dev)
		return;

	misc_deregister(&eap_device);
	_eap_dev = NULL;
	kfree(dev);
}

static void eap_free(struct usb_function *f)
{

}

static struct eap_instance *to_eap_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct eap_instance,
		func_inst.group);
}

static void eap_attr_release(struct config_item *item)
{
	struct eap_instance *fi_eap = to_eap_instance(item);

	usb_put_function_instance(&fi_eap->func_inst);
}

static struct configfs_item_operations eap_item_ops = {
	.release        = eap_attr_release,
};

static struct config_item_type eap_func_type = {
	.ct_item_ops    = &eap_item_ops,
	.ct_owner       = THIS_MODULE,
};

static struct eap_instance *to_fi_eap(struct usb_function_instance *fi)
{
	return container_of(fi, struct eap_instance, func_inst);
}

static int eap_set_inst_name(struct usb_function_instance *fi, const char *name)
{
	struct eap_instance *fi_eap;
	char *ptr;
	int name_len;

	pr_debug("%s %d\n", __func__, __LINE__);
	name_len = strlen(name) + 1;
	if (name_len > MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;

	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	fi_eap = to_fi_eap(fi);
	fi_eap->name = ptr;

	return 0;
}

static void eap_free_inst(struct usb_function_instance *fi)
{
	struct eap_instance *fi_eap;
	printk("eap free >>>>>>>>>>>>>>>>>>>>>>>\n");
	fi_eap = to_fi_eap(fi);
	kfree(fi_eap->name);
	eap_cleanup();
	//kfree(fi_eap->eap_os_desc.group.default_groups);
	kfree(fi_eap);
}

static struct usb_function_instance *eap_alloc_inst(void)
{
	struct eap_instance *fi_eap;
	int ret = 0;
//	struct usb_os_desc *descs[1];
//	char *names[1];
	printk("eap  alloc_inst >>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	fi_eap = kzalloc(sizeof(struct eap_instance), GFP_KERNEL);
	if (!fi_eap)
		return ERR_PTR(-ENOMEM);
	fi_eap->func_inst.set_inst_name = eap_set_inst_name;
	fi_eap->func_inst.free_func_inst = eap_free_inst;

	ret = eap_setup_configfs(fi_eap);
	if (ret) {
		kfree(fi_eap);
		pr_err("Error setting EAP\n");
		return ERR_PTR(ret);
	}

	config_group_init_type_name(&fi_eap->func_inst.group,
					"", &eap_func_type);

	return  &fi_eap->func_inst;
}

struct usb_function *eap_alloc(struct usb_function_instance *fi)
{
	struct eap_instance *fi_eap = to_fi_eap(fi);
	struct eap_dev *dev;
	printk("eap alloc >>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	if (fi_eap->dev == NULL) {
		return ERR_PTR(-EINVAL);
	}

	dev = fi_eap->dev;
	dev->function.name = DRIVER_NAME;
	dev->function.strings = eap_strings;

	dev->function.bind = eap_function_bind;
	dev->function.unbind = eap_function_unbind;
	dev->function.set_alt = eap_function_set_alt;
	dev->function.get_alt = eap_function_get_alt;
	dev->function.disable = eap_function_disable;
	dev->function.suspend = eap_function_disable;
	dev->function.free_func = eap_free;

	return &dev->function;
}

DECLARE_USB_FUNCTION_INIT(eap, eap_alloc_inst, eap_alloc);
MODULE_LICENSE("GPL");
