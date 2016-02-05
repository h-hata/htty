#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>  
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "htty.h"
/* Module information */
#define MODULE_NAME	"htty"
MODULE_LICENSE("GPL");

static struct proc_dir_entry *fe;
static char	buff[128];
static struct semaphore sem;
typedef struct tty_pair_t{
	int index;
	int valid;
	char	name[128];
	BUFFER	*b1;
	BUFFER	*b2;
}TTY_PAIR;
static TTY_PAIR	pair[CMINORS];

static void init_pair(void)
{
	int i;
	sema_init(&sem,1);
	for(i=0;i<CMINORS;i++){
		pair[i].valid=0;
		pair[i].name[0]='\0';
	}
}

int search_minor(char *name)
{
	int i;
	down(&sem);
	for(i=0;i<CMINORS;i++){
		if(pair[i].valid==0){
			continue;
		}
		if(strcmp(pair[i].name,name)==0){
			up(&sem);
			return i;
		}
	}
	up(&sem);
	return -1;
}

void release_minor(int index)
{
	down(&sem);
	pair[index].name[0]='\0';
	pair[index].valid='\0';
	up(&sem);
}

int hunt_minor(char *name)
{
	int i;
	//check name is unique or not
	i=search_minor(name);
	if(i>=0){
		return -2;
	}
	down(&sem);
	for(i=0;i<CMINORS;i++){
		if(pair[i].valid==0){
			pair[i].valid=1;
			strncpy(pair[i].name,name,127);
			up(&sem);
			return i;
		}
	}
	up(&sem);
	return -1;
}

static int checkName(char *name)
{
	char *ptr;
	for(ptr=name;*ptr;ptr++){
		if('a'<=*ptr && *ptr<='z') continue;
		if('A'<=*ptr && *ptr<='Z') continue;
		if('0'<=*ptr && *ptr<='9') continue;
		if(*ptr=='_' ||*ptr=='-'||*ptr=='.') continue;
		return -1;
	}
	return 0;
}
ssize_t proc_write(struct file *f,const char __user *b,size_t c,loff_t *o)
{
	char	cmd[100];
	char	name[100];
	int n;
	ssize_t ret;
	int index;
	pr_info(MODULE_NAME " proc write  in>>>: count=%zd offset=%lld\n",c,*o);
	if(c>100||c==0){
		return -ENOMEM;
	}
	memset(buff,0,128);
	if(copy_from_user(buff,b,c)){
		return -EFAULT;
	}
	*o+=c;
	ret=(ssize_t)c;
	n=sscanf(buff,"%s %s",cmd,name);
	if(n==2){
		if(checkName(name)!=0){
			 ret=-EINVAL;
			 goto err;
		}else if(strcmp(cmd,"create")==0){
			sprintf(buff,"CREATE %s",name);
			index=hunt_minor(name);
			if(index<0){
				ret=-ENOMEM;
				goto err;
			}
			if(pair[index].b1==NULL){
				pair[index].b1=kmalloc(sizeof(BUFFER),GFP_KERNEL);
				if(pair[index].b1==NULL){
					ret=-ENOMEM;
					goto err;
				}
			}
			memset(pair[index].b1,0,sizeof(BUFFER));
			if(pair[index].b2==NULL){
				pair[index].b2=kmalloc(sizeof(BUFFER),GFP_KERNEL);
				if(pair[index].b2==NULL){
					ret=-ENOMEM;
					goto err;
				}
			}
			memset(pair[index].b2,0,sizeof(BUFFER));
			n=create_htty(name,index,pair[index].b1,pair[index].b2);
			if(n<0){
				ret=-EINVAL;
				release_minor(index);
				goto err;
			}
			/*
			n=create_chtty(name,index,pair[index].b2,pair[index].b1);
			if(n>0){
				ret=-EINVAL;
				delete_htty(index);
				release_minor(index);
				goto err;
			}
			*/
		}else if(strcmp(cmd,"delete")==0){
			index=search_minor(name);
			if(index<0){
				ret=-EINVAL;
				goto err;
			}
			delete_htty(index);
			/*delete_chtty(index);*/
			release_minor(index);
			sprintf(buff,"DELETE %s",name);
		}
	}else{
		ret=-EINVAL;
	}
err:
	printk(MODULE_NAME " proc write out<<<: count=%zd offset=%lld\n",ret,*o);
	return ret;
}

ssize_t proc_read(struct file *f, char __user *b, size_t c, loff_t *o)
{
	size_t len;
	printk(MODULE_NAME "read  in>>>: count=%zd offset=%lld\n",c,*o);
	if(*o!=0){
		return (ssize_t)0;
	}
	len=strlen(buff);
	if(copy_to_user(b,buff,len+1)){
		return -EFAULT;
	}
	*o+=len+1;
	printk(MODULE_NAME "read out<<<: count=%zd offset=%lld\n",len+1,*o);
	return len+1;
}

static const struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.read = proc_read,
	.write = proc_write,
};

int init_ctl(void)
{
	pr_info("htty init_ctl  in>>>\n");
	fe=proc_create(MODULE_NAME,0666,NULL,&proc_fops);
	if(fe==NULL){
		printk(KERN_ERR "init_ctl:failed create_proc_entry");
		return -ENOMEM;
	}
	memset(buff,0,128);
	init_pair();
	pr_info("htty init_ctl  out<<<\n");
	return 0;
}
void exit_ctl(void)
{
	remove_proc_entry(MODULE_NAME,NULL);
}
EXPORT_SYMBOL(init_ctl);
EXPORT_SYMBOL(exit_ctl);
//open/closeはproc_fsがやってくれるのでread/writeだけ書けばよい



