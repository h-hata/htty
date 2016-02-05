#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include "htty.h"
/* Module information */
//MODULE_LICENSE("GPL");
#define CMAJOR	513
#define DEV_NAME	"chtty"
#define CHTTY_MINOR 0	
struct class *cclass=NULL;

int create_chtty(void)
{
	int ret=0;
	dev_t devt = MKDEV(CMAJOR,CHTTY_MINOR);
	device_create(cclass,NULL,devt,NULL,DEV_NAME);
	pr_info("htty create char device %s\n",DEV_NAME);
	return ret;
}

void delete_chtty(void)
{
	dev_t devt = MKDEV(CMAJOR,CHTTY_MINOR);
	device_destroy(cclass,devt);
	pr_info("htty delete char device minor=%d\n",CHTTY_MINOR);
}

int init_chtty(void)
{
	pr_info("htty init_cdev  in>>>\n");
	cclass=class_create(THIS_MODULE,DEV_NAME);
	pr_info("htty init_cdev  out<<<\n");
	create_chtty();
	return 0;
}
void exit_chtty(void)
{
	delete_chtty();
	pr_info("htty exit_cdev  in>>>\n");
	if(cclass){
	 class_destroy(cclass);
		pr_info("htty class_destory\n");
	}
	pr_info("htty exit_cdev  out<<<\n");
}

EXPORT_SYMBOL(init_chtty);
EXPORT_SYMBOL(exit_chtty);
EXPORT_SYMBOL(create_chtty);
EXPORT_SYMBOL(delete_chtty);

