/* ########################################################################

   mytty0tty - derived from tty0tty, with independent module/device names
   and pair mask control for DCD/CD + RI.

   Notes:
   - RTS -> peer CTS
   - DTR -> peer DSR
   - DCD/CD and RI come from per-pair external mask bits
   - pair mask bit layout:
       bit0 = side A CD/DCD   (even side of the pair)
       bit1 = side A RI
       bit2 = side B CD/DCD   (odd side of the pair)
       bit3 = side B RI

   Sysfs interface (printf '8\n' | sudo tee /sys/kernel/mytty0tty/mytnt0_mytnt1_mask ...):
       /sys/kernel/mytty0tty/mytnt0_mytnt1_mask
       /sys/kernel/mytty0tty/mytnt2_mytnt3_mask

   ######################################################################## */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#endif
#include <asm/uaccess.h>

#ifndef MYTTY_DRIVER_NAME
#define MYTTY_DRIVER_NAME	"mytty0tty"
#endif

#ifndef MYTTY_DEVICE_NAME
#define MYTTY_DEVICE_NAME	"mytnt"
#endif

#ifndef MYTTY_SYSFS_DIRNAME
#define MYTTY_SYSFS_DIRNAME	"mytty0tty"
#endif

#ifndef MYTTY_DRIVER_DESC
#define MYTTY_DRIVER_DESC	"mytty0tty null modem driver"
#endif

#define DRIVER_VERSION		"v1.5-minimal-diff"
#define DRIVER_AUTHOR		"Luis Claudio Gamboa Lopes / modified"
#define DRIVER_DESC		MYTTY_DRIVER_DESC

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

short pairs = 4;
module_param(pairs, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(pairs,
	"Number of pairs of devices to be created, maximum of 128");

#if 0
#define TTY0TTY_MAJOR		240
#define TTY0TTY_MINOR		16
#else
#define TTY0TTY_MAJOR		0
#define TTY0TTY_MINOR		0
#endif

/* fake UART values */
#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define MCR_LOOP	0x04

#define MSR_CTS		0x10
#define MSR_CD		0x20
#define MSR_DSR		0x40
#define MSR_RI		0x80

#define MYTTY_MAX_PAIRS	128
#define SIDE_A_CD_BIT	0x01
#define SIDE_A_RI_BIT	0x02
#define SIDE_B_CD_BIT	0x04
#define SIDE_B_RI_BIT	0x08

static struct tty_port *tport;
static struct kobject *mytty_kobj;

struct tty0tty_serial
{
	struct tty_struct *tty;
	int open_count;
	struct semaphore sem;
	int msr;
	int mcr;
	struct serial_struct serial;
	wait_queue_head_t wait;
	struct async_icount icount;
};

static struct tty0tty_serial **tty0tty_table;
static u8 *pair_masks;

struct mytty_pair_attr
{
	struct kobj_attribute kattr;
	int pair_index;
	char name[64];
};

static struct mytty_pair_attr *pair_attrs;

static inline int mytty_peer_index(int index)
{
	return (index % 2) ? (index - 1) : (index + 1);
}

static inline int mytty_pair_index(int index)
{
	return index / 2;
}

static inline int mytty_side_is_even(int index)
{
	return ((index & 1) == 0);
}

static unsigned int mytty_external_bits(int index)
{
	u8 mask = 0;
	int pair = mytty_pair_index(index);
	unsigned int msr = 0;

	if (pair_masks)
		mask = pair_masks[pair] & 0x0F;

	if (mytty_side_is_even(index))
	{
		if (mask & SIDE_A_CD_BIT)
			msr |= MSR_CD;
		if (mask & SIDE_A_RI_BIT)
			msr |= MSR_RI;
	}
	else
	{
		if (mask & SIDE_B_CD_BIT)
			msr |= MSR_CD;
		if (mask & SIDE_B_RI_BIT)
			msr |= MSR_RI;
	}

	return msr;
}

static unsigned int mytty_calc_msr(int index)
{
	unsigned int msr = 0;
	unsigned int mcr = 0;
	int peer = mytty_peer_index(index);

	if (tty0tty_table && tty0tty_table[peer] != NULL)
	{
		if (tty0tty_table[peer]->open_count > 0)
			mcr = tty0tty_table[peer]->mcr;
	}

	if ((mcr & MCR_RTS) == MCR_RTS)
		msr |= MSR_CTS;

	if ((mcr & MCR_DTR) == MCR_DTR)
		msr |= MSR_DSR;

	msr |= mytty_external_bits(index);
	return msr;
}

static void mytty_refresh_peer_signals(int index)
{
	int peer = mytty_peer_index(index);

	if (tty0tty_table && tty0tty_table[peer] != NULL)
	{
		if (tty0tty_table[peer]->open_count > 0)
			tty0tty_table[peer]->msr = mytty_calc_msr(peer);
	}
}

static int tty0tty_open(struct tty_struct *tty, struct file *file)
{
	struct tty0tty_serial *tty0tty;
	int index;

	tty->driver_data = NULL;
	index = tty->index;
	tty0tty = tty0tty_table[index];
	if (tty0tty == NULL)
	{
		tty0tty = kmalloc(sizeof(*tty0tty), GFP_KERNEL);
		if (!tty0tty)
			return -ENOMEM;

		memset(tty0tty, 0, sizeof(*tty0tty));
		sema_init(&tty0tty->sem, 1);
		init_waitqueue_head(&tty0tty->wait);
		tty0tty->open_count = 0;
		tty0tty_table[index] = tty0tty;
	}

	tport[index].tty = tty;
	tty->port = &tport[index];

	tty0tty->msr = mytty_calc_msr(index);
	tty0tty->mcr = 0;

	down(&tty0tty->sem);
	tty->driver_data = tty0tty;
	tty0tty->tty = tty;
	++tty0tty->open_count;
	up(&tty0tty->sem);
	return 0;
}

static void do_close(struct tty0tty_serial *tty0tty)
{
#ifdef SCULL_DEBUG
	printk(KERN_DEBUG "%s - \n", __FUNCTION__);
#endif

	down(&tty0tty->sem);
	if (!tty0tty->open_count)
	{
		up(&tty0tty->sem);
		return;
	}

	--tty0tty->open_count;
	tty0tty->mcr = 0;
	up(&tty0tty->sem);
}

static void tty0tty_close(struct tty_struct *tty, struct file *file)
{
	struct tty0tty_serial *tty0tty = tty->driver_data;

	if (tty0tty)
	{
		do_close(tty0tty);
		mytty_refresh_peer_signals(tty->index);
	}
}

static int tty0tty_write(struct tty_struct *tty, const unsigned char *buffer,
	int count)
{
	struct tty0tty_serial *tty0tty = tty->driver_data;
	int retval = 0;
	struct tty_struct *ttyx = NULL;
	int peer;

	if (!tty0tty)
		return -ENODEV;

	down(&tty0tty->sem);

	if (!tty0tty->open_count)
		goto exit;

	peer = mytty_peer_index(tty0tty->tty->index);
	if (tty0tty_table[peer] != NULL)
	{
		if (tty0tty_table[peer]->open_count > 0)
			ttyx = tty0tty_table[peer]->tty;
	}

	if (ttyx != NULL)
	{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
		tty_insert_flip_string(ttyx->port, buffer, count);
		tty_flip_buffer_push(ttyx->port);
#else
		tty_insert_flip_string(ttyx, buffer, count);
		tty_flip_buffer_push(ttyx);
#endif
		retval = count;
	}

exit:
	up(&tty0tty->sem);
	return retval;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)
static unsigned int tty0tty_write_room(struct tty_struct *tty)
#else
static int tty0tty_write_room(struct tty_struct *tty)
#endif
{
	struct tty0tty_serial *tty0tty = tty->driver_data;
	int room = 0;

	if (!tty0tty)
		return -ENODEV;

	down(&tty0tty->sem);
	if (tty0tty->open_count)
		room = 255;
	up(&tty0tty->sem);
	return room;
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

static void tty0tty_set_termios(struct tty_struct *tty,
	const struct ktermios *old_termios)
{
	unsigned int cflag;
	unsigned int iflag;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
	cflag = tty->termios.c_cflag;
	iflag = tty->termios.c_iflag;
#else
	cflag = tty->termios->c_cflag;
	iflag = tty->termios->c_iflag;
#endif

	if (old_termios)
	{
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(iflag) == RELEVANT_IFLAG(old_termios->c_iflag)))
			return;
	}
}

static int tty0tty_tiocmget(struct tty_struct *tty)
{
	struct tty0tty_serial *tty0tty = tty->driver_data;
	unsigned int result = 0;
	unsigned int msr;
	unsigned int mcr;

	if (!tty0tty)
		return -ENODEV;

	down(&tty0tty->sem);
	msr = mytty_calc_msr(tty->index);
	tty0tty->msr = msr;
	mcr = tty0tty->mcr;
	up(&tty0tty->sem);

	result = ((mcr & MCR_DTR) ? TIOCM_DTR : 0) |
		((mcr & MCR_RTS) ? TIOCM_RTS : 0) |
		((mcr & MCR_LOOP) ? TIOCM_LOOP : 0) |
		((msr & MSR_CTS) ? TIOCM_CTS : 0) |
		((msr & MSR_CD) ? TIOCM_CAR : 0) |
		((msr & MSR_RI) ? TIOCM_RI : 0) |
		((msr & MSR_DSR) ? TIOCM_DSR : 0);

	return result;
}

static int tty0tty_tiocmset(struct tty_struct *tty,
	unsigned int set, unsigned int clear)
{
	struct tty0tty_serial *tty0tty = tty->driver_data;
	unsigned int mcr;

	if (!tty0tty)
		return -ENODEV;

	down(&tty0tty->sem);
	mcr = tty0tty->mcr;

	if (set & TIOCM_RTS)
		mcr |= MCR_RTS;
	if (set & TIOCM_DTR)
		mcr |= MCR_DTR;
	if (clear & TIOCM_RTS)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_DTR)
		mcr &= ~MCR_DTR;

	tty0tty->mcr = mcr;
	up(&tty0tty->sem);

	mytty_refresh_peer_signals(tty->index);
	return 0;
}

static int tty0tty_ioctl_tiocgserial(struct tty_struct *tty,
	unsigned int cmd, unsigned long arg)
{
	struct tty0tty_serial *tty0tty = tty->driver_data;

	if (!tty0tty)
		return -ENODEV;

	if (cmd == TIOCGSERIAL)
	{
		struct serial_struct tmp;

		if (!arg)
			return -EFAULT;

		memset(&tmp, 0, sizeof(tmp));
		tmp.type = tty0tty->serial.type;
		tmp.line = tty0tty->serial.line;
		tmp.port = tty0tty->serial.port;
		tmp.irq = tty0tty->serial.irq;
		tmp.flags = ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ;
		tmp.xmit_fifo_size = tty0tty->serial.xmit_fifo_size;
		tmp.baud_base = tty0tty->serial.baud_base;
		tmp.close_delay = 5 * HZ;
		tmp.closing_wait = 30 * HZ;
		tmp.custom_divisor = tty0tty->serial.custom_divisor;
		tmp.hub6 = tty0tty->serial.hub6;
		tmp.io_type = tty0tty->serial.io_type;

		if (copy_to_user((void __user *)arg, &tmp, sizeof(struct serial_struct)))
			return -EFAULT;
		return 0;
	}
	return -ENOIOCTLCMD;
}

static int tty0tty_ioctl_tiocmiwait(struct tty_struct *tty,
	unsigned int cmd, unsigned long arg)
{
	struct tty0tty_serial *tty0tty = tty->driver_data;

	if (!tty0tty)
		return -ENODEV;

	if (cmd == TIOCMIWAIT)
	{
		DECLARE_WAITQUEUE(wait, current);
		struct async_icount cnow;
		struct async_icount cprev;

		cprev = tty0tty->icount;
		while (1)
		{
			add_wait_queue(&tty0tty->wait, &wait);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			remove_wait_queue(&tty0tty->wait, &wait);

			if (signal_pending(current))
				return -ERESTARTSYS;

			cnow = tty0tty->icount;
			if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
			    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
				return -EIO;
			if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
			    ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
			    ((arg & TIOCM_CD) && (cnow.dcd != cprev.dcd)) ||
			    ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)))
				return 0;
			cprev = cnow;
		}
	}

	return -ENOIOCTLCMD;
}

static int tty0tty_ioctl_tiocgicount(struct tty_struct *tty,
	unsigned int cmd, unsigned long arg)
{
	struct tty0tty_serial *tty0tty = tty->driver_data;

	if (!tty0tty)
		return -ENODEV;

	if (cmd == TIOCGICOUNT)
	{
		struct async_icount cnow = tty0tty->icount;
		struct serial_icounter_struct icount;

		icount.cts = cnow.cts;
		icount.dsr = cnow.dsr;
		icount.rng = cnow.rng;
		icount.dcd = cnow.dcd;
		icount.rx = cnow.rx;
		icount.tx = cnow.tx;
		icount.frame = cnow.frame;
		icount.overrun = cnow.overrun;
		icount.parity = cnow.parity;
		icount.brk = cnow.brk;
		icount.buf_overrun = cnow.buf_overrun;

		if (copy_to_user((void __user *)arg, &icount, sizeof(icount)))
			return -EFAULT;
		return 0;
	}
	return -ENOIOCTLCMD;
}

static int tty0tty_ioctl(struct tty_struct *tty,
	unsigned int cmd, unsigned long arg)
{
	switch (cmd)
	{
		case TIOCGSERIAL:
			return tty0tty_ioctl_tiocgserial(tty, cmd, arg);
		case TIOCMIWAIT:
			return tty0tty_ioctl_tiocmiwait(tty, cmd, arg);
		case TIOCGICOUNT:
			return tty0tty_ioctl_tiocgicount(tty, cmd, arg);
	}
	return -ENOIOCTLCMD;
}

static struct tty_operations serial_ops = {
	.open = tty0tty_open,
	.close = tty0tty_close,
	.write = tty0tty_write,
	.write_room = tty0tty_write_room,
	.set_termios = tty0tty_set_termios,
	.tiocmget = tty0tty_tiocmget,
	.tiocmset = tty0tty_tiocmset,
	.ioctl = tty0tty_ioctl,
};

static struct tty_driver *tty0tty_tty_driver;

static ssize_t mytty_pairmask_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	struct mytty_pair_attr *pattr;
	int pair;
	u8 value;

	pattr = container_of(attr, struct mytty_pair_attr, kattr);
	pair = pattr->pair_index;
	value = pair_masks[pair] & 0x0F;
	return scnprintf(buf, PAGE_SIZE, "%u\n", value);
}

static ssize_t mytty_pairmask_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	struct mytty_pair_attr *pattr;
	unsigned long value;
	int ret;

	pattr = container_of(attr, struct mytty_pair_attr, kattr);

	ret = kstrtoul(buf, 0, &value);
	if (ret)
		return ret;

	if (value > 0x0F)
		return -ERANGE;

	pair_masks[pattr->pair_index] = (u8)(value & 0x0F);
	return count;
}

static int mytty_create_pair_attrs(void)
{
	int i;
	int ret;

	pair_attrs = kcalloc(pairs, sizeof(*pair_attrs), GFP_KERNEL);
	if (!pair_attrs)
		return -ENOMEM;

	for (i = 0; i < pairs; i++)
	{
		int a = i * 2;
		int b = a + 1;

		pair_attrs[i].pair_index = i;
		snprintf(pair_attrs[i].name, sizeof(pair_attrs[i].name),
			"%s%d_%s%d_mask", MYTTY_DEVICE_NAME, a, MYTTY_DEVICE_NAME, b);

		sysfs_attr_init(&pair_attrs[i].kattr.attr);
		pair_attrs[i].kattr.attr.name = pair_attrs[i].name;
		pair_attrs[i].kattr.attr.mode = 0664;
		pair_attrs[i].kattr.show = mytty_pairmask_show;
		pair_attrs[i].kattr.store = mytty_pairmask_store;

		ret = sysfs_create_file(mytty_kobj, &pair_attrs[i].kattr.attr);
		if (ret)
			return ret;
	}

	return 0;
}

static void mytty_remove_pair_attrs(void)
{
	int i;

	if (!pair_attrs || !mytty_kobj)
		return;

	for (i = 0; i < pairs; i++)
		sysfs_remove_file(mytty_kobj, &pair_attrs[i].kattr.attr);

	kfree(pair_attrs);
	pair_attrs = NULL;
}

static int __init tty0tty_init(void)
{
	int retval;
	int i;

	if (pairs > MYTTY_MAX_PAIRS)
		pairs = MYTTY_MAX_PAIRS;
	if (pairs < 1)
		pairs = 1;

	tport = kmalloc_array(2 * pairs, sizeof(struct tty_port), GFP_KERNEL);
	if (!tport)
		return -ENOMEM;

	tty0tty_table = kmalloc_array(2 * pairs, sizeof(struct tty0tty_serial *), GFP_KERNEL);
	if (!tty0tty_table)
	{
		kfree(tport);
		tport = NULL;
		return -ENOMEM;
	}
	memset(tty0tty_table, 0, 2 * pairs * sizeof(struct tty0tty_serial *));

	pair_masks = kzalloc(pairs * sizeof(u8), GFP_KERNEL);
	if (!pair_masks)
	{
		kfree(tty0tty_table);
		tty0tty_table = NULL;
		kfree(tport);
		tport = NULL;
		return -ENOMEM;
	}

	mytty_kobj = kobject_create_and_add(MYTTY_SYSFS_DIRNAME, kernel_kobj);
	if (!mytty_kobj)
	{
		kfree(pair_masks);
		pair_masks = NULL;
		kfree(tty0tty_table);
		tty0tty_table = NULL;
		kfree(tport);
		tport = NULL;
		return -ENOMEM;
	}

	tty0tty_tty_driver = tty_alloc_driver(2 * pairs, 0);
	if (!tty0tty_tty_driver)
	{
		kobject_put(mytty_kobj);
		mytty_kobj = NULL;
		kfree(pair_masks);
		pair_masks = NULL;
		kfree(tty0tty_table);
		tty0tty_table = NULL;
		kfree(tport);
		tport = NULL;
		return -ENOMEM;
	}

	tty0tty_tty_driver->owner = THIS_MODULE;
	tty0tty_tty_driver->driver_name = MYTTY_DRIVER_NAME;
	tty0tty_tty_driver->name = MYTTY_DEVICE_NAME;
	tty0tty_tty_driver->major = TTY0TTY_MAJOR;
	tty0tty_tty_driver->minor_start = TTY0TTY_MINOR;
	tty0tty_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	tty0tty_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	tty0tty_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW;
	tty0tty_tty_driver->init_termios = tty_std_termios;
	tty0tty_tty_driver->init_termios.c_iflag = 0;
	tty0tty_tty_driver->init_termios.c_oflag = 0;
	tty0tty_tty_driver->init_termios.c_cflag = B38400 | CS8 | CREAD;
	tty0tty_tty_driver->init_termios.c_lflag = 0;
	tty0tty_tty_driver->init_termios.c_ispeed = 38400;
	tty0tty_tty_driver->init_termios.c_ospeed = 38400;

	tty_set_operations(tty0tty_tty_driver, &serial_ops);

	for (i = 0; i < 2 * pairs; i++)
	{
		tty_port_init(&tport[i]);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
		tty_port_link_device(&tport[i], tty0tty_tty_driver, i);
#endif
	}

	retval = tty_register_driver(tty0tty_tty_driver);
	if (retval)
	{
		printk(KERN_ERR "failed to register %s tty driver\n", MYTTY_DRIVER_NAME);
		tty_driver_kref_put(tty0tty_tty_driver);
		kobject_put(mytty_kobj);
		mytty_kobj = NULL;
		kfree(pair_masks);
		pair_masks = NULL;
		kfree(tty0tty_table);
		tty0tty_table = NULL;
		kfree(tport);
		tport = NULL;
		return retval;
	}

	retval = mytty_create_pair_attrs();
	if (retval)
	{
		printk(KERN_ERR "failed to create pair mask sysfs files for %s\n", MYTTY_DRIVER_NAME);
		tty_unregister_driver(tty0tty_tty_driver);
		tty_driver_kref_put(tty0tty_tty_driver);
		for (i = 0; i < 2 * pairs; i++)
		{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
			tty_port_destroy(&tport[i]);
#endif
		}
		kobject_put(mytty_kobj);
		mytty_kobj = NULL;
		kfree(pair_masks);
		pair_masks = NULL;
		kfree(tty0tty_table);
		tty0tty_table = NULL;
		kfree(tport);
		tport = NULL;
		return retval;
	}

	printk(KERN_INFO DRIVER_DESC " " DRIVER_VERSION "\n");
	printk(KERN_INFO "%s devices prefix: /dev/%sX\n", MYTTY_DRIVER_NAME, MYTTY_DEVICE_NAME);
	printk(KERN_INFO "%s sysfs root: /sys/kernel/%s/\n", MYTTY_DRIVER_NAME, MYTTY_SYSFS_DIRNAME);
	return 0;
}

static void __exit tty0tty_exit(void)
{
	struct tty0tty_serial *tty0tty;
	int i;

	mytty_remove_pair_attrs();

	for (i = 0; i < 2 * pairs; ++i)
	{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
		tty_port_destroy(&tport[i]);
#endif
		tty_unregister_device(tty0tty_tty_driver, i);
	}
	tty_unregister_driver(tty0tty_tty_driver);

	for (i = 0; i < 2 * pairs; ++i)
	{
		tty0tty = tty0tty_table[i];
		if (tty0tty)
		{
			while (tty0tty->open_count)
				do_close(tty0tty);
			kfree(tty0tty);
			tty0tty_table[i] = NULL;
		}
	}

	if (mytty_kobj)
	{
		kobject_put(mytty_kobj);
		mytty_kobj = NULL;
	}

	kfree(pair_masks);
	pair_masks = NULL;
	kfree(tport);
	tport = NULL;
	kfree(tty0tty_table);
	tty0tty_table = NULL;
}

module_init(tty0tty_init);
module_exit(tty0tty_exit);
