#include "time.h"
#define nr_printf printf
#define nr_gets gets
#define nr_strtol strtoul
//-------------------------------------------PNP------------------------------------------
// MB PnP configuration register

#define PNP_KEY_ADDR (0xbfd00000+0x3f0)
#define PNP_DATA_ADDR (0xbfd00000+0x3f1)



static unsigned char slave_addr;

void PNPSetConfig(char Index, char data);
char PNPGetConfig(char Index);

#define SUPERIO_CFG_REG 0x85

void EnterMBPnP(void)
{
	pcitag_t tag;
	char confval;
	tag=_pci_make_tag(VTSB_BUS,VTSB_DEV, VTSB_ISA_FUNC);
	confval=_pci_conf_readn(tag,SUPERIO_CFG_REG,1);
	_pci_conf_writen(tag,SUPERIO_CFG_REG,confval|2,1);	
}

void ExitMBPnP(void)
{
	pcitag_t tag;
	char confval,val;
	tag=_pci_make_tag(VTSB_BUS,VTSB_DEV, VTSB_ISA_FUNC);
	confval=_pci_conf_readn(tag,SUPERIO_CFG_REG,1);
	_pci_conf_writen(tag,SUPERIO_CFG_REG,confval&~2,1);	
}

void PNPSetConfig(char Index, char data)
{
	EnterMBPnP();                                // Enter IT8712 MB PnP mode
	outb(PNP_KEY_ADDR,Index);
	outb(PNP_DATA_ADDR,data);
	ExitMBPnP();
}

char PNPGetConfig(char Index)
{
	char rtn;

	EnterMBPnP();                                // Enter IT8712 MB PnP mode
	outb(PNP_KEY_ADDR,Index);
	rtn = inb(PNP_DATA_ADDR);
	ExitMBPnP();
	return rtn;
}



int dumpsis(int argc,char **argv)
{
	int i;
	volatile unsigned char *p=0xbfd003c4;
	unsigned char c;
	for(i=0;i<0x15;i++)
	{
		p[0]=i;
		c=p[1];
		printf("sr%x=0x%02x\n",i,c);
	}
	p[0]=5;
	p[1]=0x86;
	printf("after set 0x86 to sr5\n");
	for(i=0;i<0x15;i++)
	{
		p[0]=i;
		c=p[1];
		printf("sr%x=0x%02x\n",i,c);
	}
	return 0;
}

unsigned char i2cread(char slot,char offset);

union commondata{
	unsigned char data1;
	unsigned short data2;
	unsigned int data4;
	unsigned int data8[2];
	unsigned char c[8];
};
extern unsigned int syscall_addrtype;
extern int (*syscall1)(int type,long long addr,union commondata *mydata);
extern int (*syscall2)(int type,long long addr,union commondata *mydata);

static int PnpRead(int type,long long addr,union commondata *mydata)
{
	switch(type)
	{
		case 1:mydata->data1=PNPGetConfig(addr);break;
		default: return -1;break;
	}

	return 0;
}

static int PnpWrite(int type,long long addr,union commondata *mydata)
{
	switch(type)
	{
		case 1:PNPSetConfig(addr,mydata->data1);break;
		default: return -1;break;
	}

	return 0;
}

#if PCI_IDSEL_CS5536 != 0

static int logicdev=0;
static int PnpRead_w83627(int type,long long addr,union commondata *mydata)
{
	switch(type)
	{
		case 1:
			mydata->data1=w83627_read(logicdev,addr);
			break;
		default: return -1;break;
	}

	return 0;
}

static int PnpWrite_w83627(int type,long long addr,union commondata *mydata)
{
	switch(type)
	{
		case 1:
			w83627_write(logicdev,addr,mydata->data1);
			break;
		default: return -1;break;
	}

	return 0;
}

#endif


static int pnps(int argc,char **argv)
{
#if PCI_IDSEL_CS5536 != 0
		logicdev=strtoul(argv[1],0,0);
		syscall1=(void*)PnpRead_w83627;
		syscall2=(void*)PnpWrite_w83627;
#else
		syscall1=(void*)PnpRead;
		syscall2=(void*)PnpWrite;
#endif
		syscall_addrtype=0;
		return 0;
}

//
int ztetcm_index_addr=0xbfd0002e;

static int tcm_zte_io_read(int type,long long addr,union commondata *mydata)
{
		switch(type)
		{
				case 1:
						*(char *)ztetcm_index_addr=addr&0xff;
						mydata->data1=*(char *)(ztetcm_index_addr+1);
						break;
				default: return -1;break;
		}
		return 0;
}

static int tcm_zte_io_write(int type,long long addr,union commondata *mydata)
{
		switch(type)
		{
				case 1:
						*(char *)ztetcm_index_addr=addr&0xff;
						*(char *)(ztetcm_index_addr+1)=mydata->data1;
						break;
				default: return -1;break;
		}
		return 0;
}

static int tcms(int argc,char **argv)
{
	syscall1=(void*)tcm_zte_io_read;
	syscall2=(void*)tcm_zte_io_write;
	syscall_addrtype=0;
	if(argc>1)ztetcm_index_addr=strtoul(argv[1],0,0);
	return 0;
}

#include "target/via686b.h"
static int i2cslot=0;

static int DimmRead(int type,long long addr,union commondata *mydata)
{
		char i2caddr[]={(i2cslot<<1)+0xa0};
		switch(type)
		{
				case 1:
						tgt_i2cread(I2C_SINGLE,i2caddr,1,addr,&mydata->data1,1);
						break;

				default: return -1;break;
		}
		return 0;
}

static int DimmWrite(int type,long long addr,union commondata *mydata)
{
		return -1;
}


static int Ics950220Read(int type,long long addr,union commondata *mydata)
{
		char c;
		char i2caddr[]={0xd2};
		switch(type)
		{
				case 1:
						tgt_i2cread(I2C_SMB_BLOCK,i2caddr,1,addr,&mydata->data1,1);

						break;

				default: return -1;break;
		}
		return 0;
}

static int Ics950220Write(int type,long long addr,union commondata *mydata)
{
		char c;
		char i2caddr[]={0xd2};
		switch(type)
		{
				case 1:
						tgt_i2cwrite(I2C_SMB_BLOCK,i2caddr,1,addr,&mydata->data1,1);

						break;

				default: return -1;break;
		}
		return 0;
		return -1;
}

static int rom_ddr_reg_read(int type,long long addr,union commondata *mydata)
{
		char *nvrambuf;
		extern char ddr2_reg_data,_start;
		nvrambuf = 0xbfc00000+((int)&ddr2_reg_data -(int)&_start)+addr;
		//		printf("ddr2_reg_data=%x\nbuf=%x,ddr=%x\n",&ddr2_reg_data,nvrambuf,addr);
		switch(type)
		{
				case 1:memcpy(&mydata->data1,nvrambuf,1);break;
				case 2:memcpy(&mydata->data2,nvrambuf,2);break;
				case 4:memcpy(&mydata->data4,nvrambuf,4);break;
				case 8:memcpy(&mydata->data8,nvrambuf,8);break;
		}
		return 0;
}

static int rom_ddr_reg_write(int type,long long addr,union commondata *mydata)
{
		char *nvrambuf;
		char *nvramsecbuf;
		char *nvram;
		int offs;
		extern char ddr2_reg_data,_start;
		struct fl_device *dev=fl_devident(0xbfc00000,0);
		int nvram_size=dev->fl_secsize;

		nvram = 0xbfc00000+((int)&ddr2_reg_data -(int)&_start);
		offs=(int)nvram &(nvram_size - 1);
		nvram  =(int)nvram & ~(nvram_size - 1);

		/* Deal with an entire sector even if we only use part of it */

		/* If NVRAM is found to be uninitialized, reinit it. */

		/* Find end of evironment strings */
		nvramsecbuf = (char *)malloc(nvram_size);

		if(nvramsecbuf == 0) {
				printf("Warning! Unable to malloc nvrambuffer!\n");
				return(-1);
		}

		memcpy(nvramsecbuf, nvram, nvram_size);
		if(fl_erase_device(nvram, nvram_size, FALSE)) {
				printf("Error! Nvram erase failed!\n");
				free(nvramsecbuf);
				return(0);
		}

		nvrambuf = nvramsecbuf + offs;
		switch(type)
		{
				case 1:memcpy(nvrambuf+addr,&mydata->data1,1);break;
				case 2:memcpy(nvrambuf+addr,&mydata->data2,2);break;
				case 4:memcpy(nvrambuf+addr,&mydata->data4,4);break;
				case 8:memcpy(nvrambuf+addr,&mydata->data8,8);break;
		}

		if(fl_program_device(nvram, nvramsecbuf, nvram_size, FALSE)) {
				printf("Error! Nvram program failed!\n");
				free(nvramsecbuf);
				return(0);
		}
		free(nvramsecbuf);
		return 0;
}


#if defined(DEVBD2F_SM502)||defined(DEVBD2F_FIREWALL)


#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#endif

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)
#endif

void tm_binary_to_bcd(struct tm *tm)
{
		BIN_TO_BCD(tm->tm_sec);	
		BIN_TO_BCD(tm->tm_min);	
		BIN_TO_BCD(tm->tm_hour);	
		tm->tm_hour = tm->tm_hour|0x80;
		BIN_TO_BCD(tm->tm_mday);	
		BIN_TO_BCD(tm->tm_mon);	
		BIN_TO_BCD(tm->tm_year);	
		BIN_TO_BCD(tm->tm_wday);	

}

/*
 *isl 12027
 * */
char gpio_i2c_settime(struct tm *tm)
{
		struct 
		{
				char tm_sec;
				char tm_min;
				char tm_hour;
				char tm_mday;
				char tm_mon;
				char tm_year;
				char tm_wday;
				char tm_year_hi;
		}  rtcvar;
		char i2caddr[]={0xde,0};

		char a ;
		word_addr = 1;
		tm->tm_mon = tm->tm_mon + 1;
		tm_binary_to_bcd(tm);

		//when rtc stop,can't set it ,follow 5 lines to resolve it
		a = 2;
		tgt_i2cwrite(I2C_SINGLE,i2caddr,2,0x3f,&a,1);
		a = 6;
		tgt_i2cwrite(I2C_SINGLE,i2caddr,2,0x3f,&a,1);
		tgt_i2cwrite(I2C_SINGLE,i2caddr,2,0x30,&a,1);

		a = 2;
		tgt_i2cwrite(I2C_SINGLE,i2caddr,2,0x3f,&a,1);
		a = 6;
		tgt_i2cwrite(I2C_SINGLE,i2caddr,2,0x3f,&a,1);

		//begin set

		rtcvar.tm_sec=tm->tm_sec;
		rtcvar.tm_min=tm->tm_min;
		rtcvar.tm_hour=tm->tm_hour;
		rtcvar.tm_mday=tm->tm_mday;
		rtcvar.tm_mon=tm->tm_mon;
		rtcvar.tm_wday=tm->tm_wday;
		if(tm->tm_year>=0xa0)
		{
				rtcvar.tm_year = tm->tm_year - 0xa0;
				rtcvar.tm_year_hi=20;

		}
		else
		{
				rtcvar.tm_year = tm->tm_year;
				rtcvar.tm_year_hi=19;
		}
		tgt_i2cwrite(I2C_BLOCK,i2caddr,2,0x30,&rtcvar,sizeof(rtcvar));

		return 1;

}

/*
 * sm502: rx8025
 * fire:isl12027
 */




#endif

//----------------------------------------

static int syscall_i2c_type,syscall_i2c_addrlen;
static char syscall_i2c_addr[2];

static int i2c_read_syscall(int type,long long addr,union commondata *mydata)
{
		char c;
		switch(type)
		{
				case 1:
						tgt_i2cread(syscall_i2c_type,syscall_i2c_addr,syscall_i2c_addrlen,addr,&mydata->data1,1);

						break;

				default: return -1;break;
		}
		return 0;
}

static int i2c_write_syscall(int type,long long addr,union commondata *mydata)
{
		char c;
		switch(type)
		{
				case 1:
						tgt_i2cwrite(syscall_i2c_type,syscall_i2c_addr,syscall_i2c_addrlen,addr,&mydata->data1,1);

						break;

				default: return -1;break;
		}
		return 0;
		return -1;
}
//----------------------------------------

static int i2cs(int argc,char **argv)
{
		pcitag_t tag;
		volatile int i;
		if(argc<2) 
				return -1;

		i2cslot=strtoul(argv[1],0,0);

		printf("i2cslog = %d\n", i2cslot);
		switch(i2cslot)
		{
				case 0:
				case 1:
						syscall1=(void*)DimmRead;
						syscall2=(void*)DimmWrite;
						break;
				case 2:
						syscall1=(void*)Ics950220Read;
						syscall2=(void*)Ics950220Write;
						break;
				case 3:
						syscall1=(void *)rom_ddr_reg_read;
						syscall2=(void *)rom_ddr_reg_write;
						if(argc==3 && !strcmp(argv[2][2],"revert"))
						{
								extern char ddr2_reg_data,_start;
								extern char ddr2_reg_data1;
								printf("revert to default ddr setting\n");
								// tgt_flashprogram(0xbfc00000+((int)&ddr2_reg_data -(int)&_start),30*8,&ddr2_reg_data1,TRUE);
						}
						break;
				case -1:
						if(argc<4)return -1;
						syscall_i2c_type=strtoul(argv[2],0,0);
						syscall_i2c_addrlen=argc-3;
						for(i=3;i<argc;i++)syscall_i2c_addr[i-3]=strtoul(argv[i],0,0);
						syscall1=(void*)i2c_read_syscall;
						syscall2=(void*)i2c_write_syscall;
						break;
				default:
						return -1;
		}

		syscall_addrtype=0;

		return 0;
}

static unsigned char stoh(char c){
		unsigned char val;

		switch(c){
				case '0':
						val = 0x0; break;
				case '1':
						val = 0x1; break;
				case '2':
						val = 0x2; break;
				case '3':
						val = 0x3; break;
				case '4':
						val = 0x4; break;
				case '5':
						val = 0x5; break;
				case '6':
						val = 0x6; break;
				case '7':
						val = 0x7; break;
				case '8':
						val = 0x8; break;
				case '9':
						val = 0x9; break;
				case 'a':
						val = 0xa; break;
				case 'b':
						val = 0xb; break;
				case 'c':
						val = 0xc; break;
				case 'd':
						val = 0xd; break;
				case 'e':
						val = 0xe; break;
				case 'f':
						val = 0xf; break;
				default:
						val = -1; break;
		}

		return val;
}

static int test_tcm(int ac, char **av){

		int addr;
		int con = -1;
		unsigned int tag, base;
		unsigned char buf[2048] = {0};
		int len = 0;
		int i, j;
		int ret;
		static int wcount = 0; 
		static int rcount = 0;

		tag = _pci_make_tag(0, 14, 0);
		base = (_pci_conf_read(tag, 0x10)) & ~3;

		if ((ac < 6 || av[1][0] == 'h') && strcmp(av[1], "tcm_init")){
				printf("Please input < start + addr + w/r + data + stop >\n"); 
				printf("start : 1 is start, 0 is nostart;\n"); 
				printf("stop : 1 is stop, 0 is nostop \n");
				printf("addr : is slave address;\n");

				return 0;
		}

		if (!strcmp(av[1], "tcm_init")){
				tcm_init(base);

				return 0;
		}
		{
				unsigned char adr0, adr1;

				adr0 = stoh(av[2][2]);
				adr1 = stoh(av[2][3]);
				addr = ((adr0 << 4) & 0xf0)|(adr1 & 0xf);
				printf("addr = %x\n", addr);
		}

		if (!strcmp(av[3], "w")){

				i = 0; 
				while (av[4][i] != '.'){
						i++;
				}

				len = i;
				printf("write len = %x\n", len);

				for (i = 0; i < len; i+=2){
						unsigned m, n;

						m = stoh(av[4][i]);
						n = stoh(av[4][i+1]);
						buf[j] = (m << 4 &0xf0) | (n &0xf);

						printf("buf[%d] = %x\n", j, buf[j]);
						j++;
				}


				if (!strcmp(av[1], "1") && !strcmp(av[5], "1") && (!strcmp(av[2], "0x54") || !strcmp(av[2], "0x55"))){
						con = 1;
						goto run;
				}
				if (!strcmp(av[1], "1") && (!strcmp(av[2], "0x54") || !strcmp(av[2], "0x55")) && av[4][0]=='.'){
						con = 2;
						goto run;
				}
				if (!strcmp(av[1], "0") && !strcmp(av[5], "0") && (!strcmp(av[2], "0x54") || !strcmp(av[2], "0x55"))){
						con = 3;
						goto run;
				}
				if (!strcmp(av[1], "1") && !strcmp(av[5], "0") && (!strcmp(av[2], "0x54") || !strcmp(av[2], "0x55"))){
						con = 4;
						goto run;
				}

		}else if (!strcmp(av[3], "r")){

				len = atoi(av[4]);
				printf("read len : %x\n", len);

				if (!strcmp(av[5], "1")){
						con = 0;	
						goto run;
				}
		}	

run:
		switch(con){
				case 0: /* normal read */
						wcount = 0;
loopr:
						ret = tcm_read(base, addr, buf, len);
						if (ret == -1){
								if (rcount > 5){
										rcount = 0;
										printf("Error : Read Failed\n");
										goto out;
								}
								rcount++;
								goto loopr;
						}
						for (i = 0; i < len; i++){
								printf("val-%d = %x\n", i, buf[i]);
						}
						break;
				case 1: /* normal write */
loopw:
						ret = tcm_write(base, addr, buf, len);
						if (ret == -1){
								if (wcount > 5){
										wcount = 0;
										printf("Error : Write Failed\n");
										goto out;
								}
								wcount++;
								goto loopw;
						}
						break;
				case 2: /* write start */
						linux_outb(SMB_CTRL1_START, base | SMB_CTRL1);
						i2c_wait2(base);
						break;

				case 3: /* write data, no start and stop */
						{
								int i; 
								for (i = 0; i < len; i++){
										linux_outb(buf[i], base);
										i2c_wait2(base);
								}
								break;
						}

				case 4: /* write data, no stop */ 
						{ 
								int i;
								unsigned char tp;

								linux_outb(SMB_CTRL1_START, base | SMB_CTRL1);
								i2c_wait2(base);

								linux_outb(addr & 0xfe, base);
								i2c_wait2(base);

								for (i = 0; i < len; i++){
										linux_outb(buf[i], base);
										i2c_wait2(base);
								}
								break;
						}

				default:
						break;
		}

out:
		return 0;
}

static const Cmd Cmds[] =
{
		{"MyCmds"},
		{"pnps",	"", 0, "select pnp ops for d1,m1 ", pnps, 0, 99, CMD_REPEAT},
		{"tcms",	"", 0, "select tcm ops for d1,m1 ", tcms, 0, 99, CMD_REPEAT},
		{"dumpsis",	"", 0, "dump sis registers", dumpsis, 0, 99, CMD_REPEAT},
		{"test-tcm", "", 0, "read data from i2c", test_tcm, 1, 9, 0},
		{"i2cs","slotno #slot 0-1 for dimm,slot 2 for ics95220,3 for ddrcfg,3 revert for revert to default ddr setting", 0, "select i2c ops for d1,m1", i2cs, 0, 99, CMD_REPEAT},
		{0, 0}
};

#ifdef DEVBD2iF_SM502
int power_button_poll(void *unused)
{
		int cause;
		volatile int *p=0xbfe0011c;
		asm("mfc0 %0,$13":"=r"(cause));
		if(cause&(1<<10))tgt_poweroff();
		return 0;
}
#endif

static void init_cmd __P((void)) __attribute__ ((constructor));

		static void
init_cmd()
{
#ifdef DEVBD2F_SM502
		tgt_poll_register(1, power_button_poll, 0);
#endif
		cmdlist_expand(Cmds, 1);
}

