ifneq ($(KERNELRELEASE),)
obj-m := htty.o 
htty-objs:=proc.o main.o chtty.o
else
KDIR	:=	/lib/modules/`uname -r`/build
PWD	:=	`pwd`
default:
	$(MAKE)	-C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	rm -f *.o *.ko *.mod.o *.mod.c .*.cmd Module.symvers modules.order
ttest:ttest.c
	gcc -o ttest ttest.c
tar:
	(cd ..;tar cvfz tty.tgz tty/*;mv tty.tgz tty)
endif
