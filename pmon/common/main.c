/* $Id: main.c,v 1.1.1.1 2006/09/14 01:59:08 root Exp $ */

/*
 * Copyright (c) 2001-2002 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 *      This product includes software developed by Opsycon AB, Sweden.
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
 *  This code was created from code released to Public Domain
 *  by LSI Logic and Algorithmics UK.
 */ 

#include <stdio.h>
#include <string.h> 
#include <machine/pio.h>
#include <pmon.h>
#include <termio.h>
#include <endian.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef _KERNEL
#undef _KERNEL
#include <sys/ioctl.h>
#define _KERNEL
#else
#include <sys/ioctl.h>
#endif
#include <pmon.h>
#include <exec.h>
#include <file.h>

#include "mod_debugger.h"
#include "mod_symbols.h"
#include <sys/device.h>
#include "sd.h"
#include "wd.h"
#include "../../pmon/cmds/cmd_main/window.h"
#include "../../pmon/cmds/cmd_main/cmd_main.h"


#include <pflash.h>
#include <flash.h>
#include <dev/pflash_tgt.h>
extern void    *callvec;
unsigned int show_menu;
char t_dispdev[40];
char t_dispdev1[40];

#include "cmd_hist.h"		/* Test if command history selected */
#include "cmd_more.h"		/* Test if more command is selected */
#include <sys/atapi.h>		/* sata device atapi */


jmp_buf         jmpb;		/* non-local goto jump buffer */
char            line[LINESZ + 1];	/* input line */
struct termio	clntterm;	/* client terminal mode */
struct termio	consterm;	/* console terminal mode */
register_t	initial_sr;
int             memorysize;
int             memorysize_high;
char            prnbuf[LINESZ + 8];	/* commonly used print buffer */

int             repeating_cmd;
unsigned int  	moresz = 10;
#ifdef AUTOLOAD
static int autoload __P((char *));
static int run_cnt = 0;
#else
static int autorun __P((char *));
#endif
extern void __init __P((void));
extern void _exit (int retval);
extern void delay __P((int));

extern int afxIsReturnToPmon;
extern win_dp win_tp;

#ifdef INET
	static void
pmon_intr (int dummy)
{
	sigsetmask (0);
	longjmp (jmpb, 1);
}
#endif

/*FILE *logfp; = stdout; */

#if NCMD_HIST == 0
	void
get_line(char *line, int how)
{
	int i;

	i = read (STDIN, line, LINESZ);
	if(i > 0) {
		i--;
	}
	line[i] = '\0';
}
#endif
int vga_prompt(char *err, char *method, int iswait)
{
	int ic, len;

	video_set_color(0xf);

	for (ic = 0; ic < 64; ic++){
		video_putchar1(2+ic*8, REV_ROW_LINE, ' ');
		video_putchar1(2+ic*8, INF_ROW_LINE, ' ');
	}
	if(err != NULL)
	{
		len = strlen(err);
		for (ic = 0; ic < len; ic++){
			video_putchar1(2+ic*8, REV_ROW_LINE, err[ic]);
		}
	}
	if(method != NULL)
	{
		len = strlen(method);
		for (ic = 0; ic < len; ic++){
			video_putchar1(2+ic*8, INF_ROW_LINE, method[ic]);
		}
	}
	if(iswait == 1)	getchar();

	return 0;
}
int check_config (const char * file);

/*************** return  error num *********************/
/*   -1,-2,-3,-4,-5: fail to loading boot.cfg          */
/*   1: fail to loading kernel according to  boot.cfg  */
static int load_menu_list()
{
	char* rootdev = NULL;
	char* p = NULL;
	char* path = NULL;
	int i, wait = 0, err = 0;
	char *errp, *methodp;
	char err2[] = "ERROR: Can't found configure file!";//-2
	char err3[] = "ERROR: Can't read configure file!";  //-3
	char err0[] = "ERROR: Unknow error!";
	char method1[] = "WARN: Press any key to continue ...";
	char method2[] = "ERROR: Please reboot and setting BIOS ...";
const char  devs[4][64]={
"/dev/fs/ext2@usb0",
"/dev/fs/fat@usb0",
"/dev/fs/ext2@sata0",
"/dev/fs/ext2@sata1",
};
	show_menu = 1;

	if (path == NULL) {
		path = malloc(64);
		if (path == NULL) {
			path = malloc(64);
			if (path == NULL) {
				return 0;
			}
		}
	}
	memset(path, 0, 64);
        rootdev=malloc(64);
	p=getenv("bootdev");
	if(p != NULL)  
	strcpy(rootdev, p);

	sprintf(path, "%s/boot/boot.cfg", rootdev);
	if (file_exists(path)==0) {
		sprintf(path, "%s/boot.cfg", rootdev);
		if (file_exists(path)==0) {
			for(i=0;i<4;i++) {
				if(strcmp(devs[i],rootdev)==0) continue;
				sprintf(path,"%s/boot.cfg",devs[i]);
				if(file_exists(path)!=0) break;
				sprintf(path,"%s/boot/boot.cfg",devs[i]);
				if(file_exists(path)!=0) break;
			}
		}
	}
strcpy(rootdev,path);
	err = check_config(rootdev);
	if (err == 1) {
			sprintf(path, "bl -d ide %s", rootdev);
			vga_available = 1;
			err = do_cmd(path);
			if(err >= 0){
				show_menu = 0;
				video_cls();
				free(path);
				free(rootdev);
				path = NULL;

				return err;
			}
		}else
			err = -2;

	switch(err)
	{
		case -2:
			errp = err2;
			methodp = method1;
			wait = 1;
			break;
		case -3:
			errp = err3;
			methodp = method1;
			wait = 1;
			break;
		default:
			errp = err0;
	}
	show_menu = 0;
	video_cls();
	free(path);
	free(rootdev);
	path = NULL;

	vga_prompt(errp, methodp , wait);

	return err;

}

int pwd_exist(void);
int pwd_is_set(char *user);
int pwd_cmp(char *user,char* npwd);
int check_user_password()
{
	char buf[50];
	struct termio tty;
	int i;
	char c;
	if(!pwd_exist()||!pwd_is_set("user"))
		return 0;

	for(i=0;i<2;i++)
	{
		ioctl(i,TCGETA,&tty);
		tty.c_lflag &= ~ ECHO;
		ioctl(i,TCSETAW,&tty);
	}


	printf("\nPlease input user password:");
loop0:
	for(i= 0;i<50;i++)
	{
		c=getchar();
		if(c!='\n'&&c!='\r'){	
			printf("*");
			buf[i] = c;
		}
		else
		{
			buf[i]='\0';
			break;
		}
	}

	if(!pwd_cmp("user",buf))
	{
		printf("\nPassword error!\n");
		printf("Please input user password:");
		goto loop0;
	}

	for(i=0;i<2;i++)
	{
		tty.c_lflag |=  ECHO;
		ioctl(i,TCSETAW,&tty);
	}

	return 0;
}

int check_admin_password()
{
	char buf[50];
	struct termio tty;
	int i;
	char c;
	if(!pwd_exist()||!pwd_is_set("admin"))
		return 0;

	for(i=0;i<2;i++)
	{
		ioctl(i,TCGETA,&tty);
		tty.c_lflag &= ~ ECHO;
		ioctl(i,TCSETAW,&tty);
	}


	printf("\nPlease input admin password:");
loop1:
	for(i= 0;i<50;i++)
	{
		c=getchar();
		if(c!='\n'&&c!='\r'){	
			printf("*");
			buf[i] = c;
		}
		else
		{
			buf[i]='\0';
			break;
		}
	}

	if(!pwd_cmp("admin",buf))
	{
		printf("\nPassword error!\n");
		printf("Please input admin password:");
		goto loop1;
	}


	for(i=0;i<2;i++)
	{
		tty.c_lflag |=  ECHO;
		ioctl(i,TCSETAW,&tty);
	}

	return 0;
}


int check_sys_password()
{
	char buf[50];
	struct termio tty;
	int i;
	char c;
	int count=0;
	if(!pwd_exist()||!pwd_is_set("sys"))
		return 0;

	for(i=0;i<6;i++)
	{
		ioctl(i,TCGETA,&tty);
		tty.c_lflag &= ~ ECHO;
		ioctl(i,TCSETAW,&tty);
	}


	printf("\nPlease input sys password:");
loop1:
	for(i= 0;i<50;i++)
	{
		c=getchar();
		if(c!='\n'&&c!='\r'){	
			printf("*");
			buf[i] = c;
		}
		else
		{
			buf[i]='\0';
			break;
		}
	}

	if(!pwd_cmp("sys",buf))
	{
		printf("\nPassword error!\n");
		printf("Please input sys password:");
		count++;
		if(count==3)
			return -1;
		goto loop1;
	}


	for(i=0;i<6;i++)
	{
		tty.c_lflag |=  ECHO;
		ioctl(i,TCSETAW,&tty);
	}

	return 0;
}


/*
 * check file is exists. if exists return 1 ,
*/

uint8_t file_exists(char* path) {
	uint8_t i,m,n=0,ret=0,have=0;
	struct device *deva, *next_dev;
	int	fp;
	char dev[10];
	memset(dev,0,10);
	m=strlen(path);
	for(i=4;i<m;i++){
		if(path[i]=='@')break;
	}

	i++;  //找到设备名 &path[i]
	for(;i<m;i++) {
		if(path[i]=='/') 
			break;
		dev[n]=path[i];
		n++;
		if(n>9) break;
	} //dev="usb0";
	for (deva  = TAILQ_FIRST(&alldevs); deva != NULL; deva = TAILQ_NEXT(deva,dv_list)) {
		if(strcmp(deva->dv_xname,dev)==0) {
			have=1;  //看设备是不是在devls列表里
			break;
		}
	}
	if(have==0)
		return ret;
	fp = OpenLoadConfig(path);  
if(fp>0) ret=1;
close(fp);
return ret;
}
/*
 *  Main interactive command loop
 *  -----------------------------
 *
 *  Clean up removing breakpoints etc and enter main command loop.
 *  Read commands from the keyboard and execute. When executing a
 *  command this is where <crtl-c> takes us back.
 */

void __gccmain(void);
void __gccmain(void)
{
}
	int
main()
{
	char prompt[32];
	int ret = -1;
	int merr = 0;

	if (setjmp(jmpb)) {
		/* Bailing out, restore */
		closelst(0);
		ioctl(STDIN, TCSETAF, &consterm);
		printf(" break!\r\n");
	}

#ifdef INET
	signal (SIGINT, pmon_intr);
#else
	ioctl (STDIN, SETINTR, jmpb);
#endif

#if NMOD_DEBUGGER > 0
	rm_bpts();
#endif

	md_setsr(NULL, initial_sr);	/* XXX does this belong here? */


	{
		static int run=0;
		char *s = 0, *s1 = 0, *yun_init = 0;
		if(!run)
		{
			run=1;
#ifdef AUTOLOAD
		{
			int t_ide = 0;
			int t_usb = 0;
			int t_sata0 =0;
			int t_sata1 = 0;
			int t_cd = 0;
			int t_disk = 0;


#ifdef DEV2FTOPSTAR_YUN
			yun_init = getenv ("yun_init");
			if (!yun_init)
			{
				setenv("yun_init", "1");
				setenv("bootdelay", "2");
				setenv("al", "/dev/mtd0@0");
				setenv("append", "console=tty root=/dev/mtdblock2 rootfstype=yaffs2");
			}
#endif
			s = getenv ("al");
			s1 = getenv("al1");

			if (!s && !s1) {

				if ((win_tp->win_mask&0x1) == 0x1) {
					t_usb = 1;
					t_disk++;
				}

				if ((win_tp->win_mask&0x2) == 0x2) {
					t_ide = 1;
					t_disk++;
				}

				if (win_tp->sata0.w_flag == 1) {
					t_sata0 = 1;
					t_cd++;
					t_disk++;
				} else if (win_tp->sata0.w_flag == 2) {
					t_sata0 = 2;
					t_disk++;
				}

				if (win_tp->sata1.w_flag == 1) {
					t_sata1 = 1;
					t_cd++;
					t_disk++;
				} else if (win_tp->sata1.w_flag == 2) {
					t_sata1 = 2;
					t_disk++;
				}

				unsetenv("al");
				unsetenv("al1");
				unsetenv("append");
				unsetenv("append1");
				unsetenv("bootdev");
				unsetenv("bootdev1");
				unsetenv("disp");
				unsetenv("disp1");

				if (t_cd == 2) {
					setenv("al", win_tp->sata0.w_para);
					setenv("append", win_tp->sata0.w_kpara);
					setenv("bootdev", win_tp->sata0.w_para);
					setenv("disp", "SATA CDROM0");

					setenv("al1", win_tp->sata1.w_para);
					setenv("append1", win_tp->sata1.w_kpara);
					setenv("bootdev1", win_tp->sata1.w_para);
					setenv("disp1", "SATA CDROM1");
				} else if (t_cd == 1) {
					if (win_tp->sata0.w_flag == 1) {
						setenv("al", win_tp->sata0.w_para);
						setenv("append", win_tp->sata0.w_kpara);
						setenv("bootdev", win_tp->sata0.w_para);
					} else if (win_tp->sata1.w_flag == 1) {
						setenv("al", win_tp->sata1.w_para);
						setenv("append", win_tp->sata1.w_kpara);
						setenv("bootdev", win_tp->sata1.w_para);
					}
					setenv("disp", "SATA CDROM0");
					if (t_disk - t_cd >= 1) { /* priority : sata->wd->usb */
						if (t_sata0 == 2) {
							setenv("al1", win_tp->sata0.w_para);
							setenv("append1", win_tp->sata0.w_kpara);
							setenv("bootdev1", win_tp->sata0.w_para);
							setenv("disp1", "SATA DISK0");
						} else if (t_sata1 == 2) {
							setenv("al1", win_tp->sata1.w_para);
							setenv("append1", win_tp->sata1.w_kpara);
							setenv("bootdev1", win_tp->sata1.w_para);
							setenv("disp1", "SATA DISK0");
						} else if (t_ide == 1) {
							setenv("al1", win_tp->ide.w_para);
							setenv("append1", win_tp->ide.w_kpara);
							setenv("bootdev1", win_tp->ide.w_para);
							setenv("disp1", "IDE DISK");
						} else if (t_usb == 1) {
							setenv("al1", win_tp->usb.w_para);
							setenv("append1", win_tp->usb.w_kpara);
							setenv("bootdev1", win_tp->usb.w_para);
							setenv("disp1", "USB MEDIA");
						}
					}
				} else if (t_cd == 0) { /*priority sata0->sata1->ide->usb*/
					if (t_sata0 == 2) {
						setenv("al", win_tp->sata0.w_para);
						setenv("append", win_tp->sata0.w_kpara);
						setenv("bootdev", win_tp->sata0.w_para);
						setenv("disp", "SATA DISK0");

						if (t_sata1 == 2) {
							setenv("al1", win_tp->sata1.w_para);
							setenv("append1", win_tp->sata1.w_kpara);
							setenv("bootdev1", win_tp->sata1.w_para);
							setenv("disp1", "SATA DISK1");
						} else if (t_ide == 1) {
							setenv("al1", win_tp->ide.w_para);
							setenv("append1", win_tp->ide.w_kpara);
							setenv("bootdev1", win_tp->ide.w_para);
							setenv("disp1", "IDE DISK");
						} else if (t_usb == 1) {
							setenv("al1", win_tp->usb.w_para);
							setenv("append1", win_tp->usb.w_kpara);
							setenv("bootdev1", win_tp->usb.w_para);
							setenv("disp1", "USB MEDIA");
						}
					} else if (t_sata1 == 2) {
						setenv("al", win_tp->sata1.w_para);
						setenv("append", win_tp->sata1.w_kpara);
						setenv("bootdev", win_tp->sata1.w_para);
						setenv("disp", "SATA DISK0");
						if (t_ide == 1) {
							setenv("al1", win_tp->ide.w_para);
							setenv("append1", win_tp->ide.w_kpara);
							setenv("bootdev1", win_tp->ide.w_para);
							setenv("disp1", "IDE DISK");
						} else if (t_usb == 1) {
							setenv("al1", win_tp->usb.w_para);
							setenv("append1", win_tp->usb.w_kpara);
							setenv("bootdev1", win_tp->usb.w_para);
							setenv("disp1", "USB MEDIA");
						}
					} else if (t_ide == 1) {
						setenv("al", win_tp->ide.w_para);
						setenv("append", win_tp->ide.w_kpara);
						setenv("bootdev", win_tp->ide.w_para);
						setenv("disp", "IDE DISK");
						if (t_usb == 1) {
							setenv("al1", win_tp->usb.w_para);
							setenv("append1", win_tp->usb.w_kpara);
							setenv("bootdev1", win_tp->usb.w_para);
							setenv("disp1", "USB MEDIA");
						}
					} else if (t_usb == 1) {
						setenv("al", win_tp->usb.w_para);
						setenv("append", win_tp->usb.w_kpara);
						setenv("bootdev", win_tp->usb.w_para);
						setenv("disp", "USB MEDIA");
					}
				}
			}
			{
				check_user_password();

				if(afxIsReturnToPmon == 0)
				{
#ifdef ON_MENU_LIST
					merr  = load_menu_list();
#else
				merr = 1;
#endif
				}
			}

			s = getenv("al");
			s1 = getenv("al1");

			//printf("afxIsReturnToPmon :%d\n",afxIsReturnToPmon);
			if(merr == -2 || merr == -4 || merr == -3 || merr == 1)
			{
				char aut[] = " Autobooting..."; 

#ifndef DEVBD2F_SM502
				if(afxIsReturnToPmon == 0)
					vga_prompt(NULL, aut, 0);
#endif
				ret = autoload (s);
				if(ret == 1) {
					ret = autoload (s1);
				}
			}
	}

#else
			s = getenv ("autoboot");
			ret = autorun (s);
#endif
		}
#ifndef DEVBD2F_SM502
		if ((ret != 0 || s == NULL) && (afxIsReturnToPmon == 0)){
			int ic, len, count=0;
			char key;
			char err[] = " SYSTEM FILE DIDN'T FOUND OR BROKEN!";
			char method[] = " PLEASE REBOOT!";

			video_set_color(0xf);

			for (ic = 0; ic < 64; ic++){
				video_putchar1(2+ic*8, REV_ROW_LINE, ' ');
				video_putchar1(2+ic*8, INF_ROW_LINE, ' ');
			}

			len = strlen(err);
			for (ic = 0; ic < len; ic++){
				video_putchar1(2+ic*8, REV_ROW_LINE, err[ic]);
			}
			len = strlen(method);
			for (ic = 0; ic < len; ic++){
				video_putchar1(2+ic*8, INF_ROW_LINE, method[ic]);
			}
		}
#endif
	}

	while(1) {
		strncpy (prompt, getenv ("prompt"), sizeof(prompt));

#if NCMD_HIST > 0
		if (strchr(prompt, '!') != 0) {
			char tmp[8], *p;
			p = strchr(prompt, '!');
			strdchr(p);	/* delete the bang */
			sprintf(tmp, "%d", histno);
			stristr(p, tmp);
		}
#endif

		printf("%s", prompt);
#if NCMD_HIST > 0
		get_cmd(line);
#else
		get_line(line, 0);
#endif
		do_cmd(line);
		console_state(1);
	}

	free(win_tp);

	return(0);
}
#ifdef AUTOLOAD
	static int
autoload(char *s)
{
	char buf[LINESZ];
	char *pa;
	char *rd;
	unsigned int dly, lastt;
	unsigned int cnt;
	struct termio sav;
	int ret = 0;

	run_cnt++;
	if(s != NULL  && strlen(s) != 0) {
		char *d = getenv ("bootdelay");
		if(!d || !atob (&dly, d, 10) || dly < 0 || dly > 99) {
			dly = 3;
		}
#ifdef AIRWAY
	{
		volatile int *p=0xbfe0011c;
		p[1] &= ~0x8;
		p[0] |= 0x8;//gpio3 = 1
	}
#endif

#if !defined(NO_DEBUG)
		SBD_DISPLAY ("AUTO", CHKPNT_AUTO);
		printf("Press <Enter> to execute loading image:%s\n",s);
		printf("Press any other key to abort.\n");
		ioctl (STDIN, CBREAK, &sav);
		lastt = 0;
		do {
			delay(250000);
			printf ("\b\b%02d", dly);
			delay(250000);
			printf ("\b\b%02d", dly);
			delay(250000);
			printf ("\b\b%02d", --dly);
			delay(250000);
			//printf (".", --dly);
			ioctl (STDIN, FIONREAD, &cnt);
		} while (dly != 0 && cnt == 0);

		if(cnt > 0 && strchr("\n\r", getchar())) {
			cnt = 0;
		}

		ioctl (STDIN, TCSETAF, &sav);
		putchar ('\n');

		if(cnt == 0)
#endif
		{
			if(getenv("autocmd"))
			{
				strcpy(buf,getenv("autocmd"));
				do_cmd(buf);
			}
			rd= getenv("rd");
			if (rd != 0){
				sprintf(buf, "initrd %s", rd);
				do_cmd(buf);
			}

			strcpy(buf,"load ");
			strcat(buf,s);
			do_cmd(buf);

			{
				if (run_cnt == 1)
					pa = getenv("append");
				else if (run_cnt == 2)
					pa = getenv("append1");

				if (pa) {
					sprintf(buf,"g %s",pa);
				} else if((pa=getenv("karg"))) {
					sprintf(buf,"g %s",pa);
				} else {
					pa=getenv("dev");
					strcpy(buf,"g root=/dev/");
					if(pa != NULL  && strlen(pa) != 0) strcat(buf,pa);
					else strcat(buf,"sda1");
					strcat(buf," console=tty");
				}
			}

			printf("%s\n",buf);
			delay(100000);
			ret = do_cmd (buf);
		}
	}

	return ret;
}

#else
/*
 *  Handle autoboot execution
 *  -------------------------
 *
 *  Autoboot variable set. Countdown bootdelay to allow manual
 *  intervention. If CR is pressed skip counting. If var bootdelay
 *  is set use the value othervise default to 15 seconds.
 */
	static int
autorun(char *s)
{
	char buf[LINESZ];
	char *d;
	unsigned int dly, lastt;
	unsigned int cnt;
	struct termio sav;
	int ret = 0;

	if(s != NULL  && strlen(s) != 0) {
		d = getenv ("bootdelay");
		if(!d || !atob (&dly, d, 10) || dly < 0 || dly > 99) {
			dly = 15;
		}

		SBD_DISPLAY ("AUTO", CHKPNT_AUTO);
		printf("Autoboot command: \"%.60s\"\n", s);
		printf("Press <Enter> to execute or any other key to abort.\n");
		ioctl (STDIN, CBREAK, &sav);
		lastt = 0;
		dly++;
		do {
#if defined(HAVE_TOD) && defined(DELAY_INACURATE)
			time_t t;
			t = tgt_gettime ();
			if(t != lastt) {
				printf ("\r%2d", --dly);
				lastt = t;
			}
#else
			delay(1000000);
			printf ("\r%2d", --dly);
#endif
			ioctl (STDIN, FIONREAD, &cnt);
		} while (dly != 0 && cnt == 0);

		if(cnt > 0 && strchr("\n\r", getchar())) {
			cnt = 0;
		}

		ioctl (STDIN, TCSETAF, &sav);
		putchar ('\n');

		if(cnt == 0) {
			strcpy (buf, s);
			ret = do_cmd (buf);
		}
	}

	return ret;
}
#endif
/*
 *  PMON2000 entrypoint. Called after initial setup.
 */

#ifdef NO_DEBUG
#define printf /\
/
#endif

	void
dbginit (char *adr)
{
	int	memsize, freq;
	char	fs[10], *fp;

	/*	splhigh();*/

	memsize = memorysize;

	__init();	/* Do all constructor initialisation */

	SBD_DISPLAY ("ENVI", CHKPNT_ENVI);
	envinit ();

#if defined(SMP)
	/* Turn on caches unless opted out */
	if (!getenv("nocache"))
		md_cacheon();
#endif

	SBD_DISPLAY ("SBDD", CHKPNT_SBDD);
	tgt_devinit();

#ifdef INET
	SBD_DISPLAY ("NETI", CHKPNT_NETI);
	init_net (1);
#endif

#if NCMD_HIST > 0
	SBD_DISPLAY ("HSTI", CHKPNT_HSTI);
	histinit ();
#endif

#if NMOD_SYMBOLS > 0
	SBD_DISPLAY ("SYMI", CHKPNT_SYMI);
	syminit ();
#endif

#ifdef DEMO
	SBD_DISPLAY ("DEMO", CHKPNT_DEMO);
	demoinit ();
#endif

	SBD_DISPLAY ("SBDE", CHKPNT_SBDE);
	initial_sr |= tgt_enable (tgt_getmachtype ());

#ifdef SR_FR
	Status = initial_sr & ~SR_FR; /* don't confuse naive clients */
#endif
	/* Set up initial console terminal state */
	ioctl(STDIN, TCGETA, &consterm);

#ifdef HAVE_LOGO
	tgt_logo();
#else
	printf ("\n * PMON2000 Professional *"); 
#endif
	printf ("\nConfiguration [%s,%s", TARGETNAME,
			BYTE_ORDER == BIG_ENDIAN ? "EB" : "EL");
#ifdef INET
	printf (",NET");
#endif
#if NSD > 0
	printf (",SCSI");
#endif
#if NWD > 0
	printf (",IDE");
#endif
	printf ("]\nVersion: %s.\n", vers);
	printf ("Supported loaders [%s]\n", getExecString());
	printf ("Supported filesystems [%s]\n", getFSString());
	printf ("This software may be redistributed under the BSD copyright.\n");

	tgt_machprint();

	freq = tgt_pipefreq ();
	sprintf(fs, "%d", freq);
	fp = fs + strlen(fs) - 6;
	fp[3] = '\0';
	fp[2] = fp[1];
	fp[1] = fp[0];
	fp[0] = '.';
	printf (" %s MHz", fs);

	freq = tgt_cpufreq ();
	sprintf(fs, "%d", freq);
	fp = fs + strlen(fs) - 6;
	fp[3] = '\0';
	fp[2] = fp[1];
	fp[1] = fp[0];
	fp[0] = '.';
	printf (" / Bus @ %s MHz\n", fs);

	printf ("Memory size %3d MB (%3d MB Low memory, %3d MB High memory) .\n", (memsize+memorysize_high)>>20,
			(memsize>>20), (memorysize_high>>20));

	tgt_memprint();
#if defined(SMP)
	tgt_smpstartup();
#endif

	printf ("\n");

	md_clreg(NULL);
	md_setpc(NULL, (int32_t) CLIENTPC);
	md_setsp(NULL, tgt_clienttos ());
}

#ifdef NO_DEBUG
#undef printf
#endif

/*
 *  closelst(lst) -- Handle client state opens and terminal state.
 */
	void
closelst(int lst)
{
	switch (lst) {
		case 0:
			/* XXX Close all LU's opened by client */
			break;

		case 1:
			break;

		case 2:
			/* reset client terminal state to consterm value */
			clntterm = consterm;
			break;
	}
}

/*
 *  console_state(lst) -- switches between PMON2000 and client tty setting.
 */
	void
console_state(int lst)
{
	switch (lst) {
		case 1:
			/* save client terminal state and set PMON default */
			ioctl (STDIN, TCGETA, &clntterm);
			ioctl (STDIN, TCSETAW, &consterm);
			break;

		case 2:
			/* restore client terminal state */
			ioctl (STDIN, TCSETAF, &clntterm);
			break;
	}
}

/*************************************************************
 *  dotik(rate,use_ret)
 */
	void
dotik (rate, use_ret)
	int             rate, use_ret;
{
	static	int             tik_cnt;
	static	const char      more_tiks[] = "|/-\\";
	static	const char     *more_tik;

	tik_cnt -= rate;
	if (tik_cnt > 0) {
		return;
	}
	tik_cnt = 256000;
	if (more_tik == 0) {
		more_tik = more_tiks;
	}
	if (*more_tik == 0) {
		more_tik = more_tiks;
	}
	if (use_ret) {
		printf (" %c\r", *more_tik);
	} else {
		printf ("\b%c", *more_tik);
	}
	more_tik++;
}

#if NCMD_MORE == 0
/*
 *  Allow usage of more printout even if more is not compiled in.
 */
	int
more (p, cnt, size)
	char           *p;
	int            *cnt, size;
{ 
	printf("%s\n", p);
	return(0);
}
#endif

/*
 *  Non direct command placeholder. Give info to user.
 */
	int 
no_cmd(ac, av)
	int ac;
	char *av[];
{
	printf("Not a direct command! Use 'h %s' for more information.\n", av[0]);
	return (1);
}

/*
 *  Build argument area on 'clientstack' and set up clients
 *  argument registers in preparation for 'launch'.
 *  arg1 = argc, arg2 = argv, arg3 = envp, arg4 = callvector
 */

	void
initstack (ac, av, addenv)
	int ac;
	char **av;
	int addenv;
{
	char	**vsp, *ssp;
	int	ec, stringlen, vectorlen, stacklen, i;
	register_t nsp;

	/*
	 *  Calculate the amount of stack space needed to build args.
	 */
	stringlen = 0;
	if (addenv) {
		envsize (&ec, &stringlen);
	}
	else {
		ec = 0;
	}
	for (i = 0; i < ac; i++) {
		stringlen += strlen(av[i]) + 1;
	}
	stringlen = (stringlen + 3) & ~3;	/* Round to words */
	vectorlen = (ac + ec + 2) * sizeof (char *);
	stacklen = ((vectorlen + stringlen) + 7) & ~7;

	/*
	 *  Allocate stack and us md code to set args.
	 */
	nsp = md_adjstack(NULL, 0) - stacklen;
	md_setargs(NULL, ac, nsp, nsp + (ac + 1) * sizeof(char *), (int)callvec);

	/* put $sp below vectors, leaving 32 byte argsave */
	md_adjstack(NULL, nsp - 32);
	memset((void *)((int)nsp - 32), 0, 32);

	/*
	 * Build argument vector and strings on stack.
	 * Vectors start at nsp; strings after vectors.
	 */
	vsp = (char **)(int)nsp;
	ssp = (char *)((int)nsp + vectorlen);

	for (i = 0; i < ac; i++) {
		*vsp++ = ssp;
		strcpy (ssp, av[i]);
		ssp += strlen(av[i]) + 1;
	}
	*vsp++ = (char *)0;

	/* build environment vector on stack */
	if (ec) {
		envbuild (vsp, ssp);
	}
	else {
		*vsp++ = (char *)0;
	}
	/*
	 * Finally set the link register to catch returning programs.
	 */
	md_setlr(NULL, (register_t)_exit);
}

