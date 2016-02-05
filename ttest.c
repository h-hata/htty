#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
main()
{
	int s,i;
	struct termios opts;

	s=open("/dev/htty_h3",O_RDWR);
	if(s<0){
		fprintf(stderr,"Cannot open\n");
		_exit(1);
	}
	fprintf(stderr,"opened\n");
	if(tcgetattr(s,&opts)){
		perror("tcgetattr");
		_exit(1);
	}
	printf("termios.c_cflag=%011o\n",opts.c_cflag);
	cfsetispeed(&opts,B115200);
	printf("termios.c_cflag=%011o\n",opts.c_cflag);
	//tcsetattr(s,TCSANOW,&opts);
	tcsendbreak(s,0);
	//tcdrain(s);
	if(tcgetattr(s,&opts)){
		perror("tcgetattr");
		_exit(1);
	}
	printf("termios.c_cflag=%011o\n",opts.c_cflag);
	fprintf(stderr,"closing\n");
	close(s);
	return 0;
}
