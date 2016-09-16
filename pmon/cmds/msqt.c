/*
 * Copyright (c) 2010 Loongson Liu Qi
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
 *	This product includes software developed by
 *	Opsycon Open System Consulting AB, Sweden.
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

#include <stdio.h>
#include <termio.h>
#include <string.h>
#include <setjmp.h>
#include <sys/endian.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#ifdef _KERNEL
#undef _KERNEL
#include <sys/ioctl.h>
#define _KERNEL
#else
#include <sys/ioctl.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>

#include <pmon.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/*
 * Prototypes
 */
int cmd_msqt_sata __P((int, char *[]));
extern unsigned int atp_iog_base;

/*
 * Motherboard Signal Quality Test for ATP SATA Controller
 */

#define T0_TEST_REG_OFFSET 0x58
#define T1_TEST_REG_OFFSET 0xd8
#define P0_PHY_CTRL_REG_OFFSET 0x50
#define P1_PHY_CTRL_REG_OFFSET 0xd0
	int
cmd_msqt_sata(int ac, char *av[])
{
	unsigned int atp_io10_base = atp_iog_base - 0x100;
	unsigned int P0TxTEST;
	unsigned int TX_TEST_REG_OFFSET;

	/* enable sata0,sata1 phyctrl*/
	if (!strcmp(*(av + 1), "port0")){
		printf("Enable port0\n");
		outb(atp_io10_base + P0_PHY_CTRL_REG_OFFSET, 0x01);

		printf("TX is in 1.5G mode.\n");
		P0TxTEST = inl(atp_io10_base + P0_PHY_CTRL_REG_OFFSET);
		P0TxTEST = ~(0x1 << 14) & P0TxTEST; /* 1.5G */
		outl(atp_io10_base + T0_TEST_REG_OFFSET, P0TxTEST);
	}else if (!strcmp(*(av + 1), "port1")){
		printf("Enable port1\n");
		outb(atp_io10_base + P1_PHY_CTRL_REG_OFFSET, 0x01);

		printf("TX is in 1.5G mode.\n");
		P0TxTEST = inl(atp_io10_base + P1_PHY_CTRL_REG_OFFSET);
		P0TxTEST = ~(0x1 << 14) & P0TxTEST; /* 1.5G */
		outl(atp_io10_base + T1_TEST_REG_OFFSET, P0TxTEST);
	}else
		printf("Please choose correct test port\n");

	TX_TEST_REG_OFFSET = T0_TEST_REG_OFFSET;
	P0TxTEST = inl(atp_io10_base + TX_TEST_REG_OFFSET);

	/* set the tx test pattern  */
	P0TxTEST = ~(0xf << 8) & P0TxTEST;
	if(!strcmp(*(av + 2), "idle")) {
		printf("TX test pattern is idle.\n");
		P0TxTEST = 0x1 << 8 | P0TxTEST; /* idle */
	}
	else if(!strcmp(*(av + 2), "high")) {
		printf("TX test pattern is high.\n");
		P0TxTEST = 0x4 << 8 | P0TxTEST; /* high */
	}
	else if(!strcmp(*(av + 2), "low")) {
		printf("TX test pattern is low.\n");
		P0TxTEST = 0x5 << 8 | P0TxTEST; /* low */
	}
	else if(!strcmp(*(av + 2), "mix")) {
		printf("TX test pattern is mix.\n");
		P0TxTEST = 0x6 << 8 | P0TxTEST; /* mix */
	}
	else if(!strcmp(*(av + 2), "k28")) {
		printf("TX test pattern is k28.3.\n");
		P0TxTEST = 0x7 << 8 | P0TxTEST; /* k28.3 */
	}
	else if(!strcmp(*(av + 2), "align")) {
		printf("TX test pattern is align.\n");
		P0TxTEST = 0x8 << 8 | P0TxTEST; /* align*/
	}
	else if(!strcmp(*(av + 2), "cominit")) {
		printf("TX test pattern is cominit.\n");
		P0TxTEST = 0x9 << 8 | P0TxTEST; /* cominit */
	}
	else if(!strcmp(*(av + 2), "comwake")) {
		printf("TX test pattern is comwake.\n");
		P0TxTEST = 0xa << 8 | P0TxTEST; /* comwake */
	}
	else if(!strcmp(*(av + 2), "comsas")) {
		printf("TX test pattern is comsas.\n");
		P0TxTEST = 0xb << 8 | P0TxTEST; /* comsas*/
	}
	else
		printf("Please choose correct test pattern: idle/high/low/mix/k28/align/cominit/comwake/comsas.\n");

	outl(atp_io10_base + TX_TEST_REG_OFFSET, P0TxTEST);
	printf("High frequency pattern is sent to SATA0 and SATA1.\n");

	return 0;
}

static const Cmd Cmds[] =
{
	{"Motherboard Signal Quality Test (msqt)"},
	{"msqt_sata",	"port[0/1] pattern[idle/high/low/mix/k28/align/cominit/comwake/comsas]", 0, 
		"Motherboard Signal Quality Test for SATA", cmd_msqt_sata, 3, 3, 0}, 
	{0, 0}
};


static void init_cmd __P((void)) __attribute__ ((constructor));

	static void
init_cmd()
{
	cmdlist_expand(Cmds, 1);
}

