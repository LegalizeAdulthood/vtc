#include        <stdio.h>
#include	<sys/types.h>
#include	<sys/dir.h>
#include        <signal.h>
#include	<sys/time.h>
#include	<sgtty.h>
#include	<fcntl.h>
#include 	<errno.h>


/* Header bytes */
#define VT_HDR1		31
#define VT_HDR2		42

/* Client commands available */
#define VTC_QUICK	0	/* Quick read, no cksum sent */
#define VTC_OPEN	1
#define VTC_CLOSE	2
#define VTC_READ	3
#define VTC_WRITE	4
#define VTC_ZEROREAD    6

/* Errors returned */
#define VTE_NOREC	1	/* No such record available */
#define VTE_OPEN	2	/* Can't open requested block */
#define VTE_CLOSE	3	/* Can't close requested block */
#define VTE_READ	4	/* Can't read requested block */
#define VTE_WRITE	5	/* Can't write requested block */
#define VTE_NOCMD       6       /* No such command */
#define VTE_EOF         7       /* End of file: no blocks left to read */
#define VTE_ESCAPE	8	/* User aborted transfer */

#define BLKSIZE         512

/* Static things */
char vtbuf[BLKSIZE];		/* input buffer */
int record;
long block;			/* Current block */
char sum0,sum1;
int portfd;
int timeout;
char lastchar=0;
long nchar=0;
int escape=0;

#define WBUF 1024
char wbuf[WBUF];
int nwbuf=0;

vtflush()
	{
	write(portfd,wbuf,nwbuf);
	nwbuf=0;
	}

vtputc(c)
	char c;
	{
	wbuf[nwbuf++]=c;
	if(nwbuf>=WBUF)vtflush();
	}

vtputw(lo,hi)
	int lo,hi;
	{
	vtputc(lo);
	vtputc(hi);

	sum0 ^= lo;
	sum1 ^= hi;
	}




/* get a byte with timeout */

#define RBUF 1024
char rbuf[RBUF];
int nrbuf=0;
int irbuf=0;

struct itimerval start = {{0,0},{2,0}};	/* start 2 second timer */
struct itimerval stop  = {{0,0},{0,0}}; /* stop timer */

int vtgetc()
	{
	int count;
	int i;

	if(irbuf>=nrbuf)
		{
		setitimer(ITIMER_REAL,&start,0);  /* start timer */
		count = read(portfd,rbuf,RBUF);	  /* look for incoming data */ 
		setitimer(ITIMER_REAL,&stop,0);	  /* stop timer if still running */
		if(count==-1 && errno==EINTR)	  /* check for timeout */
			{
			timeout=1;
			vtputc('t');
			vtflush();
			if(nchar == 1 && lastchar == '\033')escape=1;
			nchar = 0;
			}
		if(count<=0)return -1;			

		nrbuf=count;
		irbuf=0;
		lastchar = rbuf[0];
		nchar += count;
		}
	return rbuf[irbuf++] & 255;
	}

int vtgetc0()
	{   
        register c;
    
	c = vtgetc();
	sum0 ^= c;
	return(c);
	}

int vtgetc1()
	{   
        register c;
    
	c = vtgetc();
	sum1 ^= c;
	return(c);
	}


char vtsendcmd(cmd)
	int cmd;
	{
	register i;
	char error;
	char tmp;
	int reply;

  sendcmd:
	if(escape)return VTE_ESCAPE;

	sum0=0;
	sum1=0;
	timeout=0;
	vtputw(VT_HDR1,VT_HDR2);
	vtputw(cmd,record);
	if(block<0x0000ff00L)
		{
		vtputw((short)block,(short)block>>8);
		}
	else
		{
		vtputw((short)block,255);
		vtputw((short)block>>8,(short)(block>>16));
		}

	if (cmd==VTC_WRITE)
		for(i=0; i<BLKSIZE/2; i+=2)
			vtputw(vtbuf[i],vtbuf[i+1]);

	vtputw(sum0,sum1);
	vtflush();

  	/* Now get a valid reply from the server */
  getreply:
	sum0 = 0;
	sum1 = 0;

	tmp = vtgetc0();
	if (timeout) goto sendcmd;
	if (tmp!=VT_HDR1) goto getreply;
	tmp= vtgetc1();
	if (timeout) goto sendcmd;
	if (tmp!=VT_HDR2) goto getreply;
	reply = vtgetc0();
	if (timeout) goto sendcmd;
	vtgetc1();
	if (timeout) goto sendcmd;
	vtgetc0();
	if (timeout) goto sendcmd;
	tmp= vtgetc1();
	if (timeout) goto sendcmd;
	if(tmp==-1)
		{
		vtgetc0();
		if (timeout) goto sendcmd;
		vtgetc1();
		if (timeout) goto sendcmd;
		}

			/* Retrieve the block if no errs and a READ reply */
	if (reply==VTC_READ)
		{
		for (i=0; i<BLKSIZE; i++)
			{
			vtbuf[i]= vtgetc0();
			if (timeout) goto sendcmd;
			i++;
			vtbuf[i]= vtgetc1();
			if (timeout) goto sendcmd;
      			}
    		}
			/* Get the checksum */
	vtgetc0();
	if (timeout) goto sendcmd;
	vtgetc1();
	if (timeout) goto sendcmd;

			/* Try again on a bad checksum */
	if (sum0 | sum1)
		{
		vtputc('e');
		vtflush();
		goto sendcmd;
    		}
			/* Zero the buffer if a successful zero read */
	if (reply==VTC_ZEROREAD)
		for (i=0; i<BLKSIZE; i++)
			vtbuf[i]=0;

			/* Extract any error */
	error= reply >> 4;
	return(error);
	}

struct	sgttyb	otermio;	/* original terminal characteristics */
struct	sgttyb	ntermio;	/* characteristics to use inside */

/* signal routine for timeout, does nothing, siginterruppt does the work */
int salrm()
	{
	return(0);
	}

int main(argc,argv)
	int argc;
	char **argv;
	{
	int error;

	if(argc!=2)
		{
		fprintf(stderr,"vtc <record number>\n");
		exit(-1);
		}

	record = atoi(argv[1]);

	portfd = open("/dev/tty", O_RDWR);	/* open tty port */
	if(portfd == -1)
		{
		fprintf(stderr,"Error opening /dev/tty : %s\n", strerror(errno));
		exit(1);
		}

	ioctl(portfd, TIOCGETP, &otermio);	/* set to RAW mode */
	ioctl(portfd, TIOCGETP, &ntermio);
	ntermio.sg_flags = RAW;
	ioctl(portfd, TIOCSETP, &ntermio);

	siginterrupt(SIGALRM,1);    /* allow timeout signal to abort syscall */
	signal(SIGALRM,&salrm);     /* establish timeout signal */

	block = 0;		    /* init block number (within file) */
	while(1)
		{
		error = vtsendcmd(VTC_ZEROREAD);

		if (error == VTE_EOF)break;

		if (error == VTE_ESCAPE)
			{
			fprintf(stderr,"\r\ntape operation aborted\r\n", error);
			break;
			}

		if (error != 0)
			{
			fprintf(stderr,"\r\ntape error %d\r\n", error);
			break;
			}

		write(1,vtbuf,BLKSIZE);
		block++;
    		}

	ioctl(portfd, TIOCSETP, &otermio);	/* restore terminal settings */

	return 0;
	}

