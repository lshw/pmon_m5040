/*	$Id: tgt_machdep.c,v 1.1.1.1 2006/09/14 01:59:08 root Exp $ */

/*
 * Copyright (c) 2001 Opsycon AB  (www.opsycon.se)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <include/stdarg.h>
void tgt_putchar (int);
int
tgt_printf (const char *fmt, ...)
{
    int  n;
    char buf[1024];
	char *p=buf;
	char c;
	va_list     ap;
	va_start(ap, fmt);
    n = vsprintf (buf, fmt, ap);
    va_end(ap);
	while((c = *p++))
	{ 
	 if(c == '\n')tgt_putchar('\r');
	 tgt_putchar(c);
	}
    return (n);
}

#include <sys/param.h>
#include <sys/syslog.h>
#include <machine/endian.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/intr.h>
#include <dev/pci/pcivar.h>
#include <sys/types.h>
#include <termio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <dev/ic/mc146818reg.h>
#include <dev/ic/ds15x1reg.h>
#include <linux/io.h>

#include <autoconf.h>

#include <machine/cpu.h>
#include <machine/pio.h>
#include "pflash.h"
#include "dev/pflash_tgt.h"

#include "include/bonito.h"
#include <pmon/dev/gt64240reg.h>
#include <pmon/dev/ns16550.h>

#include <pmon.h>
#include "mod_vgacon.h"
#include "mod_framebuffer.h"
#include "mod_smi502.h"

extern int kbd_initialize(void);
extern int write_at_cursor(char val);
extern const char *kbd_error_msgs[];
extern int init_win_device(void);
#include "flash.h"
#if (NMOD_FLASH_AMD + NMOD_FLASH_INTEL + NMOD_FLASH_SST) == 0
#ifdef HAVE_FLASH
#undef HAVE_FLASH
#endif
#else
#define HAVE_FLASH
#endif

#if (NMOD_X86EMU_INT10 == 0)&&(NMOD_X86EMU == 0)
int vga_available = 0;
#endif

int tgt_i2cread(int type,unsigned char *addr,int addrlen,unsigned char reg,unsigned char* buf ,int count);
int tgt_i2cwrite(int type,unsigned char *addr,int addrlen,unsigned char reg,unsigned char* buf ,int count);
extern struct trapframe DBGREG;
extern void *memset(void *, int, size_t);

int kbd_available;
int usb_kbd_available;;
int vga_available;
int bios_available = 0;

static int md_pipefreq = 0;
static int md_cpufreq = 0;
static int clk_invalid = 0;
static int nvram_invalid = 0;
static int cksum(void *p, size_t s, int set);
static void _probe_frequencies(void);

#ifndef NVRAM_IN_FLASH
void nvram_get(char *);
void nvram_put(char *);
#endif

extern int vgaterm(int op, struct DevEntry * dev, unsigned long param, int data);
extern int fbterm(int op, struct DevEntry * dev, unsigned long param, int data);
void error(unsigned long *adr, unsigned long good, unsigned long bad);
void modtst(int offset, int iter, unsigned long p1, unsigned long p2);
void do_tick(void);
void print_hdr(void);
void ad_err2(unsigned long *adr, unsigned long bad);
void ad_err1(unsigned long *adr1, unsigned long *adr2, unsigned long good, unsigned long bad);
void mv_error(unsigned long *adr, unsigned long good, unsigned long bad);

void print_err( unsigned long *adr, unsigned long good, unsigned long bad, unsigned long xor);
static inline unsigned char CMOS_READ(unsigned char addr);
static inline void CMOS_WRITE(unsigned char val, unsigned char addr);
static void init_legacy_rtc(void);

ConfigEntry	ConfigTable[] =
{
#ifdef HAVE_NB_SERIAL
	#ifdef DEVBD2F_FIREWALL
	 { (char *)LS2F_COMA_ADDR, 0, ns16550, 256, CONS_BAUD, NS16550HZ/2 },
	#else
	 { (char *)COM3_BASE_ADDR, 0, ns16550, 256, CONS_BAUD, NS16550HZ },
	#endif
#elif defined(USE_SM502_UART0)
	{ (char *)0xb6030000, 0, ns16550, 256, CONS_BAUD, NS16550HZ/2 },
#elif defined(USE_GPIO_SERIAL)
	 { (char *)COM1_BASE_ADDR, 0,ppns16550, 256, CONS_BAUD, NS16550HZ }, 
#else
	 { (char *)COM1_BASE_ADDR, 0, ns16550, 256, CONS_BAUD, NS16550HZ/2 }, 
#endif
#if NMOD_VGACON >0
	#if NMOD_FRAMEBUFFER >0
	{ (char *)1, 0, fbterm, 256, CONS_BAUD, NS16550HZ },
	#else
	{ (char *)1, 0, vgaterm, 256, CONS_BAUD, NS16550HZ },
	#endif
#endif
#ifdef DEVBD2F_FIREWALL
	 { (char *)LS2F_COMB_ADDR, 0, ns16550, 256, CONS_BAUD, NS16550HZ/2 ,1},
#endif
	{ 0 }
};

unsigned long _filebase;
extern int memorysize;
extern int memorysize_high;
extern char MipsException[], MipsExceptionEnd[];
unsigned char hwethadr[6];

void initmips(unsigned int memsz);

void addr_tst1(void);
void addr_tst2(void);
void movinv1(int iter, ulong p1, ulong p2);

pcireg_t _pci_allocate_io(struct pci_device *dev, vm_size_t size);
extern buzzer_on(void);
void
initmips(unsigned int memsz)
{
//test_gpio(1);
//	buzzer_on();
	/*enable float*/
	tgt_fpuenable();
	CPU_TLBClear();
	/*
	 *	Set up memory address decoders to map entire memory.
	 *	But first move away bootrom map to high memory.
	 */
#if 0
	GT_WRITE(BOOTCS_LOW_DECODE_ADDRESS, BOOT_BASE >> 20);
	GT_WRITE(BOOTCS_HIGH_DECODE_ADDRESS, (BOOT_BASE - 1 + BOOT_SIZE) >> 20);
#endif
	memorysize = memsz > 256 ? 256 << 20 : memsz << 20;
	memorysize_high = memsz > 256 ? (memsz - 256) << 20 : 0;

#if 0
asm(\
"	 sd %1,0x18(%0);;\n" \
"	 sd %2,0x28(%0);;\n" \
"	 sd %3,0x20(%0);;\n" \
	 ::"r"(0xffffffffbff00000ULL),"r"(memorysize),"r"(memorysize_high),"r"(0x20000000)
	 :"$2"
   );
#endif

#if 0
	{
	  int start = 0x80000000;
	  int end = 0x80000000 + 16384;

	  while (start < end) {
	    __asm__ volatile (" cache 1,0(%0)\r\n"
		              " cache 1,1(%0)\r\n"
		              " cache 1,2(%0)\r\n"
		              " cache 1,3(%0)\r\n"
		              " cache 0,0(%0)\r\n"::"r"(start));
	    start += 32;
	  }

	  __asm__ volatile ( " mfc0 $2,$16\r\n"
	                   " and  $2, $2, 0xfffffff8\r\n"
			   " or   $2, $2, 2\r\n"
			   " mtc0 $2, $16\r\n" :::"$2");
	}
#endif

	/*
	 *  Probe clock frequencys so delays will work properly.
	 */
	tgt_cpufreq();
	SBD_DISPLAY("DONE",0);
	/*
	 *  Init PMON and debug
	 */
	cpuinfotab[0] = &DBGREG;
	dbginit(NULL);

	/*
	 *  Set up exception vectors.
	 */
	SBD_DISPLAY("BEV1",0);
	bcopy(MipsException, (char *)TLB_MISS_EXC_VEC, MipsExceptionEnd - MipsException);
	bcopy(MipsException, (char *)GEN_EXC_VEC, MipsExceptionEnd - MipsException);

	CPU_FlushCache();

#ifndef ROM_EXCEPTION
	CPU_SetSR(0, SR_BOOT_EXC_VEC);
#endif
	SBD_DISPLAY("BEV0",0);
	
	printf("BEV in SR set to zero.\n");


	{
		char copyright[11] ="REV_";
		char *tmp_copy = NULL;
		int ic;

		tmp_copy = strstr(vers, "commit");
		tmp_copy  = tmp_copy  + 7;
		for(ic=4; ic<10; ic++) {
			if((*tmp_copy >= 'a') && (*tmp_copy <= 'z'))
				copyright[ic] = (*tmp_copy) - 32;
			else
				copyright[ic] = (*tmp_copy);
					tmp_copy++;
		}

		copyright[10] = '\0';
		if(getenv("bios_ver") == NULL || strcmp(getenv("bios_ver"), &copyright[4]) != 0)
		{
			setenv("bios_ver", &copyright[4]);
		}
	}

	/*
	 * Launch!
	 */
	main();
}

/*
 *  Put all machine dependent initialization here. This call
 *  is done after console has been initialized so it's safe
 *  to output configuration and debug information with printf.
 */
int afxIsReturnToPmon = 0;
extern int video_hw_init (void);

extern int fb_init(unsigned long,unsigned long);
void
tgt_devconfig()
{
#if NMOD_VGACON > 0
	int rc=1;
#if NMOD_FRAMEBUFFER > 0 
	unsigned long fbaddress,ioaddress;
	extern struct pci_device *vga_dev;
#endif
#endif
	_pci_devinit(1);	/* PCI device initialization */
#if NMOD_FRAMEBUFFER > 0
	vga_available = 0;
	if(!vga_dev) {
		printf("ERROR !!! VGA device is not found\n"); 
		rc = -1;
	}
	if (rc > 0) {
#if NMOD_SMI502 > 0
		rc = video_hw_init ();
		fbaddress  = _pci_conf_read(vga_dev->pa.pa_tag,0x10);
		ioaddress  = _pci_conf_read(vga_dev->pa.pa_tag,0x14);
		fbaddress |= 0xb0000000;
		ioaddress |= 0xb0000000;
#endif
		printf("fbaddress 0x%x\tioaddress 0x%x\n",fbaddress, ioaddress);
		fb_init(fbaddress, ioaddress);

	} else {
		printf("vga bios init failed, rc=%d\n",rc);
	}
#endif

#if (NMOD_FRAMEBUFFER > 0) || (NMOD_VGACON > 0 )
	if (rc > 0)
		if(!getenv("novga")) vga_available = 1;
		else vga_available = 0;
#endif
    config_init();
    configure();
    init_win_device();
    printf("devconfig done.\n");
#if NAND
	topstar_nand_init();
#endif
}

extern int test_icache_1(short *addr);
extern int test_icache_2(int addr);
extern int test_icache_3(int addr);
extern void godson1_cache_flush(void);
#define tgt_putchar_uc(x) (*(void (*)(char)) (((long)tgt_putchar)|0x20000000)) (x)

void
tgt_devinit()
{
	/*
	 *  Gather info about and configure caches.
	 */
	if(getenv("ocache_off")) {
		CpuOnboardCacheOn = 0;
	}
	else {
		CpuOnboardCacheOn = 1;
	}
	if(getenv("ecache_off")) {
		CpuExternalCacheOn = 0;
	}
	else {
		CpuExternalCacheOn = 1;
	}
	
    CPU_ConfigCache();

	_pci_businit(1);	/* PCI bus initialization */
}
void
tgt_reboot()
{
	unsigned long hi, lo;
	volatile int *p=0xbfe0011c;
	
	/* reset the sm502 whole chip */
	//cpu goio3
	p[1]&=~0x8;
	p[0]&=~0x8;
	p[0]|=1;

	while(1);
}


void
tgt_poweroff()
{
	volatile int *gpio_date = 0xb6010000;
	volatile int *gpio_dir = 0xb6010008;
	volatile int *p = 0xb6000040;
	int tmp;
	tmp = *p; 
	*p = tmp | 0x40;
	tmp = *gpio_date;
	*gpio_date = tmp& ~(1<<1);
	tmp = *gpio_dir;
	*gpio_dir = tmp|1<<1;
}
void
test_gpio(int i)
{
	volatile int *gpio_date = 0xb6010000;
	volatile int *gpio_dir = 0xb6010008;
	volatile int *gpio_func = 0xb6000008;
	volatile int *p = 0xb6000040;
	int tmp;
	tmp = *gpio_func;
	*gpio_func = tmp & ~ (1<<31);
	if(i==0)
	{
	tmp = *gpio_date;
	*gpio_date = tmp& ~(1<<31);
	tmp = *gpio_dir;
	*gpio_dir = tmp|1<<31;
	}
	else
	{
	tmp = *gpio_date;
	*gpio_date = tmp| (1<<31);
	tmp = *gpio_dir;
	*gpio_dir = tmp|1<<31;
	}
}

/*
 *  This function makes inital HW setup for debugger and
 *  returns the apropriate setting for the status register.
 */
register_t
tgt_enable(int machtype)
{
	/* XXX Do any HW specific setup */
	return(SR_COP_1_BIT|SR_FR_32|SR_EXL);
}

/*
 *  Target dependent version printout.
 *  Printout available target version information.
 */
void
tgt_cmd_vers()
{
}

/*
 *  Display any target specific logo.
 */
void
tgt_logo()
{
#if 0
    printf("\n");
    printf("[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[\n");
    printf("[[[            [[[[   [[[[[[[[[[   [[[[            [[[[   [[[[[[[  [[\n");
    printf("[[   [[[[[[[[   [[[    [[[[[[[[    [[[   [[[[[[[[   [[[    [[[[[[  [[\n");
    printf("[[  [[[[[[[[[[  [[[  [  [[[[[[  [  [[[  [[[[[[[[[[  [[[  [  [[[[[  [[\n");
    printf("[[  [[[[[[[[[[  [[[  [[  [[[[  [[  [[[  [[[[[[[[[[  [[[  [[  [[[[  [[\n");
    printf("[[   [[[[[[[[   [[[  [[[  [[  [[[  [[[  [[[[[[[[[[  [[[  [[[  [[[  [[\n");
    printf("[[             [[[[  [[[[    [[[[  [[[  [[[[[[[[[[  [[[  [[[[  [[  [[\n");
    printf("[[  [[[[[[[[[[[[[[[  [[[[[  [[[[[  [[[  [[[[[[[[[[  [[[  [[[[[  [  [[\n");
    printf("[[  [[[[[[[[[[[[[[[  [[[[[[[[[[[[  [[[  [[[[[[[[[[  [[[  [[[[[[    [[\n");
    printf("[[  [[[[[[[[[[[[[[[  [[[[[[[[[[[[  [[[   [[[[[[[[   [[[  [[[[[[[   [[\n");
    printf("[[  [[[[[[[[[[[[[[[  [[[[[[[[[[[[  [[[[            [[[[  [[[[[[[[  [[\n");
    printf("[[[[[[[2005][[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[\n"); 
#endif
}

static void init_legacy_rtc(void)
{
	int year, month, date, hour, min, sec;
	int i;

    year = CMOS_READ(DS_REG_YEAR);
    month = CMOS_READ(DS_REG_MONTH);
    date = CMOS_READ(DS_REG_DATE);
    hour = CMOS_READ(DS_REG_HOUR);
    min = CMOS_READ(DS_REG_MIN);
    sec = CMOS_READ(DS_REG_SEC);
    if( (year > 99) || (month < 1 || month > 12) ||
                (date < 1 || date > 31) || (hour > 23) || (min > 59) ||
                (sec > 59) ){

        tgt_printf("RTC time invalid, reset to epoch.\n");
#ifdef DEVBD2F_FIREWALL
	{
		struct tm tm;
		time_t t;
		clk_invalid = 0;
		tm.tm_sec = 0;
		tm.tm_min = 0;
		tm.tm_hour = 0;
		tm.tm_mday = 1;
		tm.tm_mon = 0;
		tm.tm_year = 108;
		t = mktime(&tm);
		tgt_settime(t);
		clk_invalid = 1;
	}
#else
        CMOS_WRITE(3, DS_REG_YEAR);
        CMOS_WRITE(1, DS_REG_MONTH);
        CMOS_WRITE(1, DS_REG_DATE);
        CMOS_WRITE(0, DS_REG_HOUR);
        CMOS_WRITE(0, DS_REG_MIN);
        CMOS_WRITE(0, DS_REG_SEC);
#endif
        }
	tgt_printf("RTC: %02d-%02d-%02d %02d:%02d:%02d\n",
	   year, month, date, hour, min, sec);
}

inline unsigned char W8319_READ(unsigned char addr)
{
    unsigned char val;
	char i2caddr[] = {(unsigned char)0xd2};

	tgt_i2cread(I2C_SINGLE,i2caddr,1,addr,&val,1);
	val &=0xff;

    return val;
}

inline void W8319_WRITE(unsigned char val, unsigned char addr)
{
	char a;
	char i2caddr[] = {(unsigned char)0xd2};

	val &=0xff;
	tgt_i2cwrite(I2C_SINGLE,i2caddr,1,addr,&val,1);

}

/*
 *   if watch dog is running,this value is "1".
 *   The value of Watch Dog Timer defined "WDTIMER",and the resolution is 250 ms.
 */

#define WDTIMER 0xf0  //  60 s = 250 ms * 0xf0
static int watch_dog;

int watch_dog_enable()
{
	unsigned char s,m;

	s=W8319_READ(6);
	s |=0x40;
	W8319_WRITE(s,6);
	W8319_WRITE(WDTIMER,7);
#ifdef WDDBUG
	s=W8319_READ(6);
	m=W8319_READ(7);
	printf("Watch dog is enable:%02x,%02x\n",s,m);
#else
	printf("Watch dog is enable\n");
#endif
	watch_dog=1;

	return 0;

}

int watch_dog_disable()
{
	unsigned char s,m;

	s=W8319_READ(6);
	s &=~0x40;
	W8319_WRITE(s,6);
	W8319_WRITE(WDTIMER,7);
#ifdef WDDBUG
	s=W8319_READ(6);
	m=W8319_READ(7);
	printf("Watch dog is disable:%02x,%02x\n",s,m);
#else
	printf("Watch dog is disable\n");
#endif

	watch_dog=0;

	return 0;

}

int watch_dog_feed()
{
	unsigned char m;

	if(!watch_dog)
	{
		printf("ERR:Watch dog is stopped,don't feed!\n");
		return -1;
	}

	W8319_WRITE(WDTIMER,7);
#ifdef WDDBUG
	m=W8319_READ(7);
	printf("feed Watch dog :%02x\n",m);
#else
	printf("feed Watch dog\n");
#endif

	return 0;

}

int get_watch_dog_status()
{
	return watch_dog;
}

void show_watch_dog_status()
{

	unsigned char m;

	if(watch_dog)
	{
		printf("Watch dog is running.\n");
		m=W8319_READ(7);
		printf("Watch dog timer is %d ms\n",m*250);
	}
	else
		printf("Watch dog is stopped.\n");

}


int word_addr;
static inline unsigned char CMOS_READ(unsigned char addr)
{
    unsigned char val;
    unsigned char tmp1,tmp2;
	volatile int tmp;
	pcitag_t tag;
	unsigned char value;
	char i2caddr[] = {(unsigned char)0xd0};
	if(addr >= 0x0a)
		return 0;
	value = value | 0x20;
	tgt_i2cread(I2C_SINGLE,i2caddr,1,addr,&val,1);
	tmp1 = ((val >> 4) & 0x0f)*10;
	tmp2  = val & 0x0f;
	val = tmp1 + tmp2;

    return val;
}

static inline void CMOS_WRITE(unsigned char val, unsigned char addr)
{
	char a;
  	unsigned char tmp1,tmp2;
	volatile int tmp;
	char i2caddr[] = {(unsigned char)0xd0};
	tmp1 = (val / 10) << 4;
	tmp2  = (val % 10);
	val = tmp1 | tmp2;
	if(addr >= 0x0a)
		return 0;
	{
		unsigned char value;
		value = value | 0x20;
		tgt_i2cwrite(I2C_SINGLE,i2caddr,1,addr,&val,1);
	}
}

static void
_probe_frequencies()
{
#ifdef HAVE_TOD
	int i, timeout, cur, sec, cnt;
#endif
    SBD_DISPLAY ("FREQ", CHKPNT_FREQ);

#if 0
    md_pipefreq = 300000000;        /* Defaults */
    md_cpufreq  = 66000000;
#else
    md_pipefreq = 660000000;        /* NB FPGA*/
    md_cpufreq  = 60000000;
#endif

    clk_invalid = 1;
#ifdef HAVE_TOD
#ifdef DEVBD2F_FIREWALL 
	{
		extern void __main();
		char tmp;
		char i2caddr[] = {0xde};
		tgt_i2cread(I2C_SINGLE,i2caddr,2,0x3f,&tmp,1);
		/*
		 * bit0:Battery is run out ,please replace the rtc battery
		 * bit4:Rtc oscillator is no operating,please reset the machine
		 */
		tgt_printf("0x3f value  %x\n",tmp);
		init_legacy_rtc();
		tgt_i2cread(I2C_SINGLE,i2caddr,2,0x14,&tmp,1);
		tgt_printf("0x14 value  %x\n",tmp);
	}
#else
    init_legacy_rtc();
#endif

    SBD_DISPLAY ("FREI", CHKPNT_FREQ);

        /*
         * Do the next twice for two reasons. First make sure we run from
         * cache. Second make sure synched on second update. (Pun intended!)
         */
aa:
	for(i = 2;  i != 0; i--) {
		cnt = CPU_GetCOUNT();
        timeout = 10000000;
        while(CMOS_READ(DS_REG_CTLA) & DS_CTLA_UIP);
			sec = CMOS_READ(DS_REG_SEC);
            do {
				timeout--;
                while(CMOS_READ(DS_REG_CTLA) & DS_CTLA_UIP);
					cur = CMOS_READ(DS_REG_SEC);
			} while(timeout != 0 && ((cur == sec)||(cur != ((sec + 1) % 60))) && (CPU_GetCOUNT() - cnt < 0x30000000));
		cnt = CPU_GetCOUNT() - cnt;
        if(timeout == 0) {
			tgt_printf("time out!\n");
            break;          /* Get out if clock is not running */
        }
    }
	/*
	 *  Calculate the external bus clock frequency.
	 */
	if (timeout != 0) {
		clk_invalid = 0;
		md_pipefreq = cnt / 10000;
		md_pipefreq *= 20000;
		/* we have no simple way to read multiplier value
		 */
		md_cpufreq = 66000000;
	}
    tgt_printf("cpu fre %u\n",md_pipefreq);
#endif /* HAVE_TOD */
}

/*
 *   Returns the CPU pipelie clock frequency
 */
int
tgt_pipefreq()
{
	if(md_pipefreq == 0) {
		_probe_frequencies();
	}
	return(md_pipefreq);
}

/*
 *   Returns the external clock frequency, usually the bus clock
 */
int
tgt_cpufreq()
{
	if(md_cpufreq == 0) {
		_probe_frequencies();
	}
	return(md_cpufreq);
}


time_t
tgt_gettime()
{
	struct tm tm;
    int ctrlbsave;
    time_t t;

      tgt_i2cinit1();
	/*gx 2005-01-17 */
	//return 0;
#ifdef HAVE_TOD
    if(!clk_invalid) {
//		ctrlbsave = CMOS_READ(DS_REG_CTLB);
 //       CMOS_WRITE(ctrlbsave | DS_CTLB_SET, DS_REG_CTLB);
		tm.tm_sec = CMOS_READ(DS_REG_SEC);
        tm.tm_min = CMOS_READ(DS_REG_MIN);
        tm.tm_hour = CMOS_READ(DS_REG_HOUR);
        tm.tm_wday = CMOS_READ(DS_REG_WDAY);
        tm.tm_mday = CMOS_READ(DS_REG_DATE);
        tm.tm_mon = CMOS_READ(DS_REG_MONTH) - 1;
        tm.tm_year = CMOS_READ(DS_REG_YEAR);
        if(tm.tm_year < 50)tm.tm_year += 100;
//			CMOS_WRITE(ctrlbsave & ~DS_CTLB_SET, DS_REG_CTLB);
            tm.tm_isdst = tm.tm_gmtoff = 0;
            t = gmmktime(&tm);
        }
        else
#endif
		{
            t = 957960000;  /* Wed May 10 14:00:00 2000 :-) */
        }
    return(t);
}

char gpio_i2c_settime(struct tm *tm);
/*
 *  Set the current time if a TOD clock is present
 */
void
tgt_settime(time_t t)
{
	struct tm *tm;
    int ctrlbsave;

	//return ;
#ifdef HAVE_TOD
    if(!clk_invalid) {
		tm = gmtime(&t);
	#ifndef DEVBD2F_FIREWALL
        ctrlbsave = CMOS_READ(DS_REG_CTLB);
        CMOS_WRITE(ctrlbsave | DS_CTLB_SET, DS_REG_CTLB);
        CMOS_WRITE(tm->tm_year % 100, DS_REG_YEAR);
        CMOS_WRITE(tm->tm_mon + 1, DS_REG_MONTH);
        CMOS_WRITE(tm->tm_mday, DS_REG_DATE);
        CMOS_WRITE(tm->tm_wday, DS_REG_WDAY);
        CMOS_WRITE(tm->tm_hour, DS_REG_HOUR);
        CMOS_WRITE(tm->tm_min, DS_REG_MIN);
        CMOS_WRITE(tm->tm_sec, DS_REG_SEC);
        CMOS_WRITE(ctrlbsave & ~DS_CTLB_SET, DS_REG_CTLB);
	#else
		gpio_i2c_settime(tm);
	#endif
    }
#endif
}

/*
 *  Print out any target specific memory information
 */
void
tgt_memprint()
{
	printf("Primary Instruction cache size %dkb (%d line, %d way)\n",
		CpuPrimaryInstCacheSize / 1024, CpuPrimaryInstCacheLSize, CpuNWayCache);
	printf("Primary Data cache size %dkb (%d line, %d way)\n",
		CpuPrimaryDataCacheSize / 1024, CpuPrimaryDataCacheLSize, CpuNWayCache);
	if(CpuSecondaryCacheSize != 0) {
		printf("Secondary cache size %dkb\n", CpuSecondaryCacheSize / 1024);
	}
	if(CpuTertiaryCacheSize != 0) {
		printf("Tertiary cache size %dkb\n", CpuTertiaryCacheSize / 1024);
	}
}

void
tgt_machprint()
{
	printf("Copyright 2000-2002, Opsycon AB, Sweden.\n");
	printf("Copyright 2005, ICT CAS.\n");
	printf("CPU %s @", md_cpuname());
} 

/*
 *  Return a suitable address for the client stack.
 *  Usually top of RAM memory.
 */

register_t
tgt_clienttos()
{
	return((register_t)(int)PHYS_TO_UNCACHED(memorysize & ~7) - 64);
}

#ifdef HAVE_FLASH
/*
 *  Flash programming support code.
 */

/*
 *  Table of flash devices on target. See pflash_tgt.h.
 */

struct fl_map tgt_fl_map_boot16[]={
	TARGET_FLASH_DEVICES_16
};

struct fl_map *
tgt_flashmap()
{
	return tgt_fl_map_boot16;
}   
void
tgt_flashwrite_disable()
{
}

int
tgt_flashwrite_enable()
{
	return(1);
}

void
tgt_flashinfo(void *p, size_t *t)
{
	struct fl_map *map;

	map = fl_find_map(p);
	if(map) {
		*t = map->fl_map_size;
	}
	else {
		*t = 0;
	}
}

void
tgt_flashprogram(void *p, int size, void *s, int endian)
{
	printf("Programming flash %x:%x into %x\n", s, size, p);
	if(fl_erase_device(p, size, TRUE)) {
		printf("Erase failed!\n");
		return;
	}
	if(fl_program_device(p, s, size, TRUE)) {
		printf("Programming failed!\n");
	}
	fl_verify_device(p, s, size, TRUE);
}
#endif /* PFLASH */

/*
 *  Network stuff.
 */
void
tgt_netinit()
{
}

int
tgt_ethaddr(char *p)
{
	bcopy((void *)&hwethadr, p, 6);
	return(0);
}

void
tgt_netreset()
{
}

/*************************************************************************/
/*
 *	Target dependent Non volatile memory support code
 *	=================================================
 *
 *
 *  On this target a part of the boot flash memory is used to store
 *  environment. See EV64260.h for mapping details. (offset and size).
 */

/*
 *  Read in environment from NV-ram and set.
 */
void
tgt_mapenv(int (*func) __P((char *, char *)))
{
	char *ep;
	char env[512];
	char *nvram;
	int i;

    /*
    *  Check integrity of the NVRAM env area. If not in order
    *  initialize it to empty.
    */
	printf("in envinit\n");
#ifdef NVRAM_IN_FLASH
    nvram = (char *)(tgt_flashmap())->fl_map_base;
	printf("nvram=%08x\n",(unsigned int)nvram);
	if(fl_devident(nvram, NULL) == 0 ||
           cksum(nvram + NVRAM_OFFS, NVRAM_SIZE, 0) != 0) {
#else
    nvram = (char *)malloc(512);
	nvram_get(nvram);
	if(cksum(nvram, NVRAM_SIZE, 0) != 0) {
#endif
		printf("NVRAM is invalid!\n");
        nvram_invalid = 1;
    }
    else {
		nvram += NVRAM_OFFS;
        ep = nvram+2;;

        while(*ep != 0) {
			char *val = 0, *p = env;
			i = 0;
            while((*p++ = *ep++) && (ep <= nvram + NVRAM_SIZE - 1) && i++ < 255) {
				if((*(p - 1) == '=') && (val == NULL)) {
					*(p - 1) = '\0';
                    val = p;
                }
            }
            if(ep <= nvram + NVRAM_SIZE - 1 && i < 255) {
				(*func)(env, val);
            }
            else {
				nvram_invalid = 2;
                break;
		    }
        }
    }

	printf("NVRAM@%x\n",(u_int32_t)nvram);

	/*
	 *  Ethernet address for Galileo ethernet is stored in the last
	 *  six bytes of nvram storage. Set environment to it.
	 */
	bcopy(&nvram[ETHER_OFFS], hwethadr, 6);
	sprintf(env, "%02x:%02x:%02x:%02x:%02x:%02x", hwethadr[0], hwethadr[1],
	    hwethadr[2], hwethadr[3], hwethadr[4], hwethadr[5]);
	(*func)("ethaddr", env);

#ifndef NVRAM_IN_FLASH
	free(nvram);
#endif

#ifdef no_thank_you
    (*func)("vxWorks", env);
#endif

	sprintf(env, "%d", memorysize / (1024 * 1024));
	(*func)("memsize", env);

	sprintf(env, "%d", memorysize_high / (1024 * 1024));
	(*func)("highmemsize", env);

	sprintf(env, "%d", md_pipefreq);
	(*func)("cpuclock", env);

	sprintf(env, "%d", md_cpufreq);
	(*func)("busclock", env);

	(*func)("systype", SYSTYPE);
	
}

int
tgt_unsetenv(char *name)
{
    char *ep, *np, *sp;
    char *nvram;
    char *nvrambuf;
    char *nvramsecbuf;
	int status;
    if(nvram_invalid) {
	    return(0);
    }

	/* Use first defined flash device (we probably have only one) */
#ifdef NVRAM_IN_FLASH
    nvram = (char *)(tgt_flashmap())->fl_map_base;

	/* Map. Deal with an entire sector even if we only use part of it */
    nvram += NVRAM_OFFS & ~(NVRAM_SECSIZE - 1);
	nvramsecbuf = (char *)malloc(NVRAM_SECSIZE);
	if(nvramsecbuf == 0) {
		printf("Warning! Unable to malloc nvrambuffer!\n");
		return(-1);
	}
    memcpy(nvramsecbuf, nvram, NVRAM_SECSIZE);
	nvrambuf = nvramsecbuf + (NVRAM_OFFS & (NVRAM_SECSIZE - 1));
#else
    nvramsecbuf = nvrambuf = nvram = (char *)malloc(512);
	nvram_get(nvram);
#endif

    ep = nvrambuf + 2;
	status = 0;
    while((*ep != '\0') && (ep <= nvrambuf + NVRAM_SIZE)) {
		np = name;
        sp = ep;
        while((*ep == *np) && (*ep != '=') && (*np != '\0')) {
			ep++;
            np++;
		}
        if((*np == '\0') && ((*ep == '\0') || (*ep == '='))) {
			while(*ep++);
                while(ep <= nvrambuf + NVRAM_SIZE) {
                    *sp++ = *ep++;
                }
        if(nvrambuf[2] == '\0') {
             nvrambuf[3] = '\0';
        }
        cksum(nvrambuf, NVRAM_SIZE, 1);
#ifdef NVRAM_IN_FLASH
        if(fl_erase_device(nvram, NVRAM_SECSIZE, FALSE)) {
			status = -1;
			break;
        }

        if(fl_program_device(nvram, nvramsecbuf, NVRAM_SECSIZE, FALSE)) {
			status = -1;
			break;
        }
#else
		nvram_put(nvram);
#endif
		status = 1;
		break;
        }
        else if(*ep != '\0') {
             while(*ep++ != '\0');
        }
    }
	free(nvramsecbuf);
    return(status);
}

int
tgt_setenv(char *name, char *value)
{
	char *ep;
    int envlen;
    char *nvrambuf;
    char *nvramsecbuf;
#ifdef NVRAM_IN_FLASH
    char *nvram;
#endif
	/* Non permanent vars. */
	if(strcmp(EXPERT, name) == 0) {
		return(1);
	}

    /* Calculate total env mem size requiered */
    envlen = strlen(name);
    if(envlen == 0) {
       return(0);
    }
    if(value != NULL) {
		envlen += strlen(value);
    }
    envlen += 2;    /* '=' + null byte */
    if(envlen > 255) {
         return(0);      /* Are you crazy!? */
    }

	/* Use first defined flash device (we probably have only one) */
#ifdef NVRAM_IN_FLASH
    nvram = (char *)(tgt_flashmap())->fl_map_base;

	/* Deal with an entire sector even if we only use part of it */
    nvram += NVRAM_OFFS & ~(NVRAM_SECSIZE - 1);
#endif

        /* If NVRAM is found to be uninitialized, reinit it. */
	if(nvram_invalid) {
		nvramsecbuf = (char *)malloc(NVRAM_SECSIZE);
		if(nvramsecbuf == 0) {
			printf("Warning! Unable to malloc nvrambuffer!\n");
			return(-1);
		}
#ifdef NVRAM_IN_FLASH
		memcpy(nvramsecbuf, nvram, NVRAM_SECSIZE);
#endif
		nvrambuf = nvramsecbuf + (NVRAM_OFFS & (NVRAM_SECSIZE - 1));
                memset(nvrambuf, -1, NVRAM_SIZE);
                nvrambuf[2] = '\0';
                nvrambuf[3] = '\0';
                cksum((void *)nvrambuf, NVRAM_SIZE, 1);
		printf("Warning! NVRAM checksum fail. Reset!\n");
#ifdef NVRAM_IN_FLASH
                if(fl_erase_device(nvram, NVRAM_SECSIZE, FALSE)) {
			printf("Error! Nvram erase failed!\n");
			free(nvramsecbuf);
                        return(-1);
                }
                if(fl_program_device(nvram, nvramsecbuf, NVRAM_SECSIZE, FALSE)) {
			printf("Error! Nvram init failed!\n");
			free(nvramsecbuf);
                        return(-1);
                }
#else
		nvram_put(nvramsecbuf);
#endif
        nvram_invalid = 0;
		free(nvramsecbuf);
        }

        /* Remove any current setting */
        tgt_unsetenv(name);

        /* Find end of evironment strings */
	nvramsecbuf = (char *)malloc(NVRAM_SECSIZE);
	if(nvramsecbuf == 0) {
		printf("Warning! Unable to malloc nvrambuffer!\n");
		return(-1);
	}
#ifndef NVRAM_IN_FLASH
	nvram_get(nvramsecbuf);
#else
        memcpy(nvramsecbuf, nvram, NVRAM_SECSIZE);
#endif
	nvrambuf = nvramsecbuf + (NVRAM_OFFS & (NVRAM_SECSIZE - 1));
	/* Etheraddr is special case to save space */
	if (strcmp("ethaddr", name) == 0) {
		char *s = value;
		int i;
		int32_t v;
		for(i = 0; i < 6; i++) {
			gethex(&v, s, 2);
			hwethadr[i] = v;
			s += 3;         /* Don't get to fancy here :-) */
		} 
	} else {
		ep = nvrambuf + 2;
		if(*ep != '\0') {
			do {
				while(*ep++ != '\0');
			} while(*ep++ != '\0');
			ep--;
		}
		if(((int)ep + NVRAM_SIZE - (int)ep) < (envlen + 1)) {
			free(nvramsecbuf);
			return(0);      /* Bummer! */
		}

		/*
		 *  Special case heaptop must always be first since it
		 *  can change how memory allocation works.
		 */
		if(strcmp("heaptop", name) == 0) {

			bcopy(nvrambuf + 2, nvrambuf + 2 + envlen,
				 ep - nvrambuf + 1);

			ep = nvrambuf + 2;
			while(*name != '\0') {
				*ep++ = *name++;
			}
			if(value != NULL) {
				*ep++ = '=';
				while((*ep++ = *value++) != '\0');
			}
			else {
				*ep++ = '\0';
			}
		}
		else {
			while(*name != '\0') {
				*ep++ = *name++;
			}
			if(value != NULL) {
				*ep++ = '=';
				while((*ep++ = *value++) != '\0');
			}
			else {
				*ep++ = '\0';
			}
			*ep++ = '\0';   /* End of env strings */
		}
	}
    cksum(nvrambuf, NVRAM_SIZE, 1);

	bcopy(hwethadr, &nvramsecbuf[ETHER_OFFS], 6);
#ifdef NVRAM_IN_FLASH
        if(fl_erase_device(nvram, NVRAM_SECSIZE, FALSE)) {
		printf("Error! Nvram erase failed!\n");
		free(nvramsecbuf);
                return(0);
        }
        if(fl_program_device(nvram, nvramsecbuf, NVRAM_SECSIZE, FALSE)) {
		printf("Error! Nvram program failed!\n");
		free(nvramsecbuf);
                return(0);
        }
#else
	nvram_put(nvramsecbuf);
#endif
	free(nvramsecbuf);
    return(1);
}

/*
 *  Calculate checksum. If 'set' checksum is calculated and set.
 */
static int
cksum(void *p, size_t s, int set)
{
	u_int16_t sum = 0;
	u_int8_t *sp = p;
	int sz = s / 2;

	if(set) {
		*sp = 0;	/* Clear checksum */
		*(sp + 1) = 0;	/* Clear checksum */
	}
	while(sz--) {
		sum += (*sp++) << 8;
		sum += *sp++;
	}
	if(set) {
		sum = -sum;
		*(u_int8_t *)p = sum >> 8;
		*((u_int8_t *)p + 1) = sum;
	}
	return(sum);
}

#ifndef NVRAM_IN_FLASH

/*
 *  Read and write data into non volatile memory in clock chip.
 */
void
nvram_get(char *buffer)
{
	int i;
	for(i = 0; i < 114; i++) {
		linux_outb(i + RTC_NVRAM_BASE, RTC_INDEX_REG);	/* Address */
		buffer[i] = linux_inb(RTC_DATA_REG);
	}
}

void
nvram_put(char *buffer)
{
	int i;
	for(i = 0; i < 114; i++) {
		linux_outb(i + RTC_NVRAM_BASE, RTC_INDEX_REG);	/* Address */
		linux_outb(buffer[i],RTC_DATA_REG);
	}
}

#endif

/*
 *  Simple display function to display a 4 char string or code.
 *  Called during startup to display progress on any feasible
 *  display before any serial port have been initialized.
 */
void
tgt_display(char *msg, int x)
{
	/* Have simple serial port driver */
	tgt_putchar(msg[0]);
	tgt_putchar(msg[1]);
	tgt_putchar(msg[2]);
	tgt_putchar(msg[3]);
	tgt_putchar('\r');
	tgt_putchar('\n');
}

void
clrhndlrs()
{
}

int
tgt_getmachtype()
{
	return(md_cputype());
}

/*
 *  Create stubs if network is not compiled in
 */
#ifdef INET
void
tgt_netpoll()
{
	splx(splhigh());
}

#else
extern void longjmp(label_t *, int);
void gsignal(label_t *jb, int sig);
void
gsignal(label_t *jb, int sig)
{
	if(jb != NULL) {
		longjmp(jb, 1);
	}
};

int	netopen (const char *, int);
int	netread (int, void *, int);
int	netwrite (int, const void *, int);
long netlseek (int, long, int);
int	netioctl (int, int, void *);
int	netclose (int);
int netopen(const char *p, int i)	{ return -1;}
int netread(int i, void *p, int j)	{ return -1;}
int netwrite(int i, const void *p, int j)	{ return -1;}
int netclose(int i)	{ return -1;}
long int netlseek(int i, long j, int k)	{ return -1;}
int netioctl(int j, int i, void *p)	{ return -1;}
void tgt_netpoll()	{};

#endif /*INET*/

#define SPINSZ		0x800000
#define DEFTESTS	7
#define MOD_SZ		20
#define BAILOUT		if (bail) goto skip_test;
#define BAILR		if (bail) return;

/* memspeed operations */
#define MS_BCOPY	1
#define MS_COPY		2
#define MS_WRITE	3
#define MS_READ		4
#include "mycmd.c"

#ifdef DEVBD2F_FIREWALL
#include "i2c-sm502.c"
#elif defined(DEVBD2F_SM502)
#include "i2c-sm502.c"
#endif
