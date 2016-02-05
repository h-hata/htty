/*
 * HTTY driver
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2 of the License.
 *
 */

//#include <linux/config.h>
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
#include <linux/sysfs.h>
#include <asm/uaccess.h>
#include "htty.h"

#define DRIVER_VERSION " v1.0"
#define DRIVER_AUTHOR "Hiroaki Hata<hata@qc5.so-net.ne.jp>"
#define DRIVER_DESC "htty"

/* Module information */ MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

#define DELAY_TIME		HZ * 2	/* 2 seconds per character */
#define H_TTY_MAJOR		240	/* experimental range */

struct htty_serial {
	char	name[128];
	struct tty_struct	*tty;		/* pointer to the tty for this device */
	struct tty_port *port;
	int			open_count;	/* number of times this port has been opened */
	struct semaphore	sem;		/* locks this structure */
	int minor;
	/* for tiocmget and tiocmset functions */
	int			msr;		/* MSR shadow */
	int			mcr;		/* MCR shadow */

	/* for ioctl fun */
	struct serial_struct	serial;
	wait_queue_head_t	wait;
	struct termios termios;
	struct async_icount	icount;
};
static struct htty_serial *htty_table[CMINORS];/* initially all NULL */
static struct tty_port tty_port[CMINORS];
static int tty_open(struct tty_struct *tty, struct file *file)
{
	struct htty_serial *htty;
	int index;

	/* initialize the pointer in case something fails */
	tty->driver_data = NULL;
	pr_info(DRIVER_DESC " open in index = %d <<<\n",tty->index);
	/* get the serial object associated with this tty pointer */
	index = tty->index;
	htty = htty_table[index];
	if (htty == NULL) {
		return -ENOMEM;
	}	
	down(&htty->sem);
	/* save our structure within the tty structure */
	tty->driver_data = htty;
	htty->tty = tty;
	htty->port=&tty_port[index];
	++htty->open_count;
	up(&htty->sem);
	pr_info(DRIVER_DESC " open out >>>\n");
	return 0;
}
static void do_close(struct htty_serial *htty)
{
	down(&htty->sem);
	if (!htty->open_count) {
		/* port was never opened */
		goto exit;
	}
	--htty->open_count;
exit:
	up(&htty->sem);
	pr_info(DRIVER_DESC " close device=%s minor=%d\n",htty->name,htty->minor);
}

static void tty_close(struct tty_struct *tty, struct file *file)
{
	int index = tty->index;
	struct htty_serial *htty = htty_table[index];
	if (htty){
		do_close(htty);
	}
}	

static int htty_write(struct tty_struct *tty, 
		      const unsigned char *buffer, int count)
{
	struct htty_serial *htty = tty->driver_data;
	int i;
	int retval = -EINVAL;

	if (!htty)
		return -ENODEV;

	down(&htty->sem);

	if (!htty->open_count)
		/* port was not opened */
		goto exit;

	/* fake sending the data out a hardware port by
	 * writing it to the kernel debug log.
	 */
	printk(KERN_DEBUG "%s - ", __FUNCTION__);
	for (i = 0; i < count; ++i)
		printk("%02x ", buffer[i]);
	printk("\n");
		
exit:
	up(&htty->sem);
	return retval;
}

static int htty_write_room(struct tty_struct *tty) 
{
	struct htty_serial *htty = tty->driver_data;
	int room = -EINVAL;

	if (!htty)
		return -ENODEV;

	down(&htty->sem);
	
	if (!htty->open_count) {
		/* port was not opened */
		goto exit;
	}

	/* calculate how much room is left in the device */
	room = 255;

exit:
	up(&htty->sem);
	return room;
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))
static int http_ioctl_tcsets(struct tty_struct *tty,
                      unsigned int cmd, unsigned long arg)
{
	struct termios *ptr;
	struct htty_serial *htty;
	int index = tty->index;
	unsigned int cflag;
	int ret;

	cflag = tty->termios.c_cflag;
	pr_info(DRIVER_DESC " ioctl tcsets tty->termios.cflag:%011o\n",cflag);
	ptr=(struct termios *)arg;
	if(ptr==NULL){
		pr_info(DRIVER_DESC " ioctl tcsets arg is null\n");
		return -ENOIOCTLCMD;
	}
	cflag=ptr->c_cflag;
	pr_info(DRIVER_DESC " ioctl tcsets arg->cflag:%011o\n",cflag);
	htty = htty_table[index];
	if (htty!=NULL) {
		ret=copy_from_user(&htty->termios,ptr,sizeof(struct termios));
	}
	return 0;
}

static void htty_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
	unsigned int cflag;
	cflag = tty->termios.c_cflag;
	pr_info(DRIVER_DESC " set_termios cflag:%011o\n",cflag );

	/* check that they really want us to change something */
	if (old_termios) {
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(tty->termios.c_iflag) == 
		     RELEVANT_IFLAG(old_termios->c_iflag))) {
			pr_info(DRIVER_DESC " - nothing to change...\n");
			return;
		}
	}

	/* get the byte size */
	switch (cflag & CSIZE) {
		case CS5:
			printk(KERN_DEBUG " - data bits = 5\n");
			break;
		case CS6:
			printk(KERN_DEBUG " - data bits = 6\n");
			break;
		case CS7:
			printk(KERN_DEBUG " - data bits = 7\n");
			break;
		default:
		case CS8:
			printk(KERN_DEBUG " - data bits = 8\n");
			break;
	}
	
	/* determine the parity */
	if (cflag & PARENB)
		if (cflag & PARODD)
			printk(KERN_DEBUG " - parity = odd\n");
		else
			printk(KERN_DEBUG " - parity = even\n");
	else
		printk(KERN_DEBUG " - parity = none\n");

	/* figure out the stop bits requested */
	if (cflag & CSTOPB)
		printk(KERN_DEBUG " - stop bits = 2\n");
	else
		printk(KERN_DEBUG " - stop bits = 1\n");

	/* figure out the hardware flow control settings */
	if (cflag & CRTSCTS)
		printk(KERN_DEBUG " - RTS/CTS is enabled\n");
	else
		printk(KERN_DEBUG " - RTS/CTS is disabled\n");
	
	/* determine software flow control */
	/* if we are implementing XON/XOFF, set the start and 
	 * stop character in the device */
	if (I_IXOFF(tty) || I_IXON(tty)) {
		unsigned char stop_char  = STOP_CHAR(tty);
		unsigned char start_char = START_CHAR(tty);

		/* if we are implementing INBOUND XON/XOFF */
		if (I_IXOFF(tty))
			printk(KERN_DEBUG " - INBOUND XON/XOFF is enabled, "
				"XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			printk(KERN_DEBUG" - INBOUND XON/XOFF is disabled");

		/* if we are implementing OUTBOUND XON/XOFF */
		if (I_IXON(tty))
			printk(KERN_DEBUG" - OUTBOUND XON/XOFF is enabled, "
				"XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			printk(KERN_DEBUG" - OUTBOUND XON/XOFF is disabled");
	}

	/* get the baud rate wanted */
	printk(KERN_DEBUG " - baud rate = %d", tty_get_baud_rate(tty));
}

/* Our fake UART values */
#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define MCR_LOOP	0x04
#define MSR_CTS		0x08
#define MSR_CD		0x10
#define MSR_RI		0x20
#define MSR_DSR		0x40

static int htty_tiocmget(struct tty_struct *tty)
{
	struct htty_serial *htty = tty->driver_data;
	unsigned int result = 0;
	unsigned int msr = htty->msr;
	unsigned int mcr = htty->mcr;
	pr_info(DRIVER_DESC "   tiocmget:\n");

	result = ((mcr & MCR_DTR)  ? TIOCM_DTR  : 0) |	/* DTR is set */
           ((mcr & MCR_RTS)  ? TIOCM_RTS  : 0) |	/* RTS is set */
           ((mcr & MCR_LOOP) ? TIOCM_LOOP : 0) |	/* LOOP is set */
           ((msr & MSR_CTS)  ? TIOCM_CTS  : 0) |	/* CTS is set */
           ((msr & MSR_CD)   ? TIOCM_CAR  : 0) |	/* Carrier detect is set*/
           ((msr & MSR_RI)   ? TIOCM_RI   : 0) |	/* Ring Indicator is set */
           ((msr & MSR_DSR)  ? TIOCM_DSR  : 0);	/* DSR is set */
	return result;
}

static int htty_tiocmset(struct tty_struct *tty,
                         unsigned int set, unsigned int clear)
{
	struct htty_serial *htty = tty->driver_data;
	unsigned int mcr = htty->mcr;
	pr_info(DRIVER_DESC "   tiocmset:\n");

	if (set & TIOCM_RTS)
		mcr |= MCR_RTS;
	if (set & TIOCM_DTR)
		mcr |= MCR_RTS;

	if (clear & TIOCM_RTS)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_DTR)
		mcr &= ~MCR_RTS;

	/* set the new MCR value in the device */
	htty->mcr = mcr;
	return 0;
}

int htty_read_proc(char *page, char **start, off_t off, int count,
                          int *eof, void *data)
{
	struct htty_serial *htty;
	off_t begin = 0;
	int length = 0;
	int i;

	length += sprintf(page, "htty serinfo:1.0 driver:%s\n", DRIVER_VERSION);
	for (i = 0; i < CMINORS && length < PAGE_SIZE; ++i) {
		htty = htty_table[i];
		if (htty == NULL)
			continue;

		length += sprintf(page+length, "%d\n", i);
		if ((length + begin) > (off + count))
			goto done;
		if ((length + begin) < off) {
			begin += length;
			length = 0;
		}
	}
	*eof = 1;
done:
	if (off >= (length + begin))
		return 0;
	*start = page + (off-begin);
	return (count < begin+length-off) ? count : begin + length-off;
}

static int htty_ioctl_tiocgserial(
		struct tty_struct *tty, unsigned int cmd, unsigned long arg)
{
	struct htty_serial *htty = tty->driver_data;
	pr_info(DRIVER_DESC "   ->ioctl_tiocgserial:\n");
	if (cmd == TIOCGSERIAL) {
		struct serial_struct tmp;

		if (!arg)
			return -EFAULT;

		memset(&tmp, 0, sizeof(tmp));

		tmp.type		= htty->serial.type;
		tmp.line		= htty->serial.line;
		tmp.port		= htty->serial.port;
		tmp.irq			= htty->serial.irq;
		tmp.flags		= ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ;
		tmp.xmit_fifo_size	= htty->serial.xmit_fifo_size;
		tmp.baud_base		= htty->serial.baud_base;
		tmp.close_delay		= 5*HZ;
		tmp.closing_wait	= 30*HZ;
		tmp.custom_divisor	= htty->serial.custom_divisor;
		tmp.hub6		= htty->serial.hub6;
		tmp.io_type		= htty->serial.io_type;

		if (copy_to_user((void __user *)arg, &tmp, sizeof(struct serial_struct)))
			return -EFAULT;
		return 0;
	}
	return -ENOIOCTLCMD;
}
static int htty_ioctl_tiocgicount(
		struct tty_struct *tty, unsigned int cmd, unsigned long arg)
{
	struct htty_serial *htty = tty->driver_data;
	pr_info(DRIVER_DESC "   ->ioctl_tiocgicount:\n");

	if (cmd == TIOCGICOUNT) {
		struct async_icount cnow = htty->icount;
		struct serial_icounter_struct icount;

		icount.cts	= cnow.cts;
		icount.dsr	= cnow.dsr;
		icount.rng	= cnow.rng;
		icount.dcd	= cnow.dcd;
		icount.rx	= cnow.rx;
		icount.tx	= cnow.tx;
		icount.frame	= cnow.frame;
		icount.overrun	= cnow.overrun;
		icount.parity	= cnow.parity;
		icount.brk	= cnow.brk;
		icount.buf_overrun = cnow.buf_overrun;

		if (copy_to_user((void __user *)arg, &icount, sizeof(icount)))
			return -EFAULT;
		return 0;
	}
	return -ENOIOCTLCMD;
}

static int http_ioctl_tcgets(struct tty_struct *tty,
                      unsigned int cmd, unsigned long arg)
{
	struct termios *ptr;
	struct htty_serial *htty;
	int index = tty->index;
	int ret=0;
	ptr=(struct termios *)arg;
	if(ptr==NULL){
		return -ENOIOCTLCMD;
	}
	htty = htty_table[index];
	if (htty!=NULL) {
		ret=copy_to_user((struct termios *)arg,&htty->termios,
				sizeof(struct termios));
	}
	pr_info(DRIVER_DESC " ioctl tcgets tty->termios.cflag:%011o\n",
			htty->termios.c_cflag);
	return ret;
}




/* the real htty_ioctl function.  The above is done to get the small functions in the book */
static int htty_ioctl(struct tty_struct *tty,
                      unsigned int cmd, unsigned long arg)
{
	pr_info(DRIVER_DESC " ioctl cmd:%X\n",cmd );
	switch (cmd) {
	case TCGETS://0x5401
		return http_ioctl_tcgets(tty,cmd,arg);
	case TCSETS://0x5402
		return http_ioctl_tcsets(tty,cmd,arg);
	case TCSETSW://0x5403 送信バッファがフラッシュ後設定
		return http_ioctl_tcsets(tty,cmd,arg);
	case TIOCGSERIAL:
		return htty_ioctl_tiocgserial(tty, cmd, arg);
//	case TIOCMIWAIT:
//		return htty_ioctl_tiocmiwait(tty, file, cmd, arg);
	case TIOCGICOUNT:
		return htty_ioctl_tiocgicount(tty, cmd, arg);
	}
	return -ENOIOCTLCMD;
}

static struct tty_driver *tty_driver;
void delete_htty(int index)
{
	struct htty_serial *htty;
	htty = htty_table[index];
	if (htty) {
		// close the port 
		while (htty->open_count)
			do_close(htty);
		// shut down our timer and free the memory
		pr_info(DRIVER_DESC " delete device %s minor=%d\n",htty->name,index);
		kfree(htty);
		htty_table[index] = NULL;
		tty_unregister_device(tty_driver,index);
	}
}
int create_htty(char *name,int index,BUFFER *rbuf,BUFFER *wbuf)
{
	char	ptr[120];
	struct htty_serial *htty;
	if(index<0 || index>=CMINORS){
		return -1;
	}
	/* first time accessing this device, let's create it */
	htty=htty_table[index];
	if(htty==NULL){
		htty = kmalloc(sizeof(*htty), GFP_KERNEL);
		if (!htty)
			return -ENOMEM;
		//init_MUTEX(&htty->sem);
		sema_init(&htty->sem,1);
		htty_table[index] = htty;
	}
	memcpy(&htty->termios,&tty_std_termios,sizeof(struct termios));
	htty->termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	htty->open_count = 0;
	htty->minor=index;
	tty_port_init(&tty_port[index]);
	sprintf(ptr,"htty_%s",name);
	tty_driver->name=ptr;
	sprintf(htty->name,"%s",ptr);
	htty->port=&tty_port[index];
	tty_port_register_device(&tty_port[index],tty_driver,index, NULL);
	pr_info(DRIVER_DESC " create device %s minor=%d\n",ptr,index);
	return 0;
}

int  (*install)(struct tty_driver *driver, struct tty_struct *tty);
void remove(struct tty_driver *driver, struct tty_struct *tty){
	pr_info(DRIVER_DESC " remove\n");
}
void shutdown(struct tty_struct *tty){
	pr_info(DRIVER_DESC " shutdown\n");
}
void cleanup(struct tty_struct *tty){
	pr_info(DRIVER_DESC " cleanup\n");
}
int  (*put_char)(struct tty_struct *tty, unsigned char ch);
void (*flush_chars)(struct tty_struct *tty);
int  (*chars_in_buffer)(struct tty_struct *tty);
long (*compat_ioctl)(struct tty_struct *tty,unsigned int cmd, unsigned long arg);
void throttle(struct tty_struct * tty){
	pr_info(DRIVER_DESC " throttle\n");
}
void unthrottle(struct tty_struct * tty){
	pr_info(DRIVER_DESC " unthrottle\n");
}
void stop(struct tty_struct *tty){
	pr_info(DRIVER_DESC " stop\n");
}
void start(struct tty_struct *tty){
	pr_info(DRIVER_DESC " start\n");
}
void hangup(struct tty_struct *tty){
	pr_info(DRIVER_DESC " hang_up\n");
}
int (*break_ctl)(struct tty_struct *tty, int state);
void flush_buffer(struct tty_struct *tty){
	pr_info(DRIVER_DESC " flush_buffer\n");
}
void set_ldisc(struct tty_struct *tty){
	pr_info(DRIVER_DESC " set_ldisk\n");
}
void wait_until_sent(struct tty_struct *tty, int timeout){
	pr_info(DRIVER_DESC " wait_until_sent %d\n",timeout);
}
void send_xchar(struct tty_struct *tty, char ch){
	pr_info(DRIVER_DESC " send_xchar\n");
}
int (*resize)(struct tty_struct *tty, struct winsize *ws);
int set_termiox(struct tty_struct *tty, struct termiox *tnew)
{
	pr_info(DRIVER_DESC " set_termiox\n");
	return 0;
}
int (*get_icount)(struct tty_struct *tty, struct serial_icounter_struct *icount);

static struct tty_operations serial_ops = {
	.open = tty_open,
	.close = tty_close,
	.write = htty_write,
	.write_room = htty_write_room,
	.set_termios = htty_set_termios,
	.set_termiox = set_termiox,
	.tiocmget = htty_tiocmget,
	.tiocmset = htty_tiocmset,
	.ioctl = htty_ioctl,
	.throttle=throttle,
	.unthrottle=unthrottle,
	.stop=stop,
	.start=start,
	.hangup=hangup,
	//.remove=remove,
	.shutdown=shutdown,
	.cleanup=cleanup,
	.flush_buffer=flush_buffer,
	.set_ldisc=set_ldisc,
	.wait_until_sent=wait_until_sent,
	.send_xchar=send_xchar,
	};

static int __init htty_init(void)
{
	int retval;
	int i;
	pr_info(DRIVER_DESC " init in>>>\n");
	/* allocate the tty driver */
	tty_driver = alloc_tty_driver(CMINORS);
	if (!tty_driver)
		return -ENOMEM;

	/* initialize the tty driver */
	tty_driver->owner = THIS_MODULE;
	tty_driver->driver_name = "htty";
	tty_driver->name = "htty";
	tty_driver->major = H_TTY_MAJOR,
	tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	tty_driver->subtype = SERIAL_TYPE_NORMAL,
	tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_driver->flags |= TTY_DRIVER_UNNUMBERED_NODE;
	tty_driver->init_termios = tty_std_termios;
	tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	tty_set_operations(tty_driver, &serial_ops);

	// register the tty driver
	retval = tty_register_driver(tty_driver);
	if (retval) {
		printk(KERN_ERR "failed to register htty tty driver");
		put_tty_driver(tty_driver);
		return retval;
	}
	init_ctl();
	init_chtty();
	for(i=0;i<CMINORS;i++){
		htty_table[i]=NULL;
	}
	pr_info(DRIVER_DESC " init out<<<\n");
	pr_info(DRIVER_DESC DRIVER_VERSION "\n");
	return retval;
}

static void __exit htty_exit(void)
{
	int i;
	pr_info(DRIVER_DESC " rmmod\n" );

	/* shut down all of the timers and free the memory */

	for (i = 0; i < CMINORS; ++i) {
		delete_htty(i);
	}
	exit_ctl();
	exit_chtty();
	tty_unregister_driver(tty_driver);
}


EXPORT_SYMBOL(create_htty);
EXPORT_SYMBOL(delete_htty);
module_init(htty_init);
module_exit(htty_exit);

