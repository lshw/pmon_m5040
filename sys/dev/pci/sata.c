/*
 * Copyright (C) 2008 Freescale Semiconductor, Inc.
 *		Dave Liu <daveliu@freescale.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#define DEBUG_RX
#include "bpfilter.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <ctype.h>

#if defined(__NetBSD__) || defined(__OpenBSD__)

#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>


#if defined(__OpenBSD__)
#endif

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>


#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_fxpreg.h>

#else /* __FreeBSD__ */

#include <sys/sockio.h>


#include <vm/vm.h>              /* for vtophys */
#include <vm/vm_param.h>        /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */

#include <pci/pcivar.h>

#endif /* __NetBSD__ || __OpenBSD__ */

#if NGZIP > 0
#include <gzipfs.h>
#endif /* NGZIP */

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/ata/atavar.h>
#include <dev/ata/atareg.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

#include <linux/libata.h>
#include <fis.h>
#include <part.h>
#include <pmon.h>
#include "sata_8620.h"
#include <sys/atapi.h>

#define ISO_DEFUALT_BLOCK_SIZE 512
#define ISO_BLOCK_SIZE ISO_DEFUALT_BLOCK_SIZE * 4

#define CFG_SATA_MAX_DEVICE 2
extern block_dev_desc_t sata_dev_desc[CFG_SATA_MAX_DEVICE];
#define CFG_HZ 660000000
#define debug(fmt,args...)	printf(fmt ,##args)
extern vm_offset_t _pci_dmamap __P((vm_offset_t, u32));
#define vtophys(p)      _pci_dmamap((vm_offset_t)p, 1)
#define VA_TO_PA(x)	UNCACHED_TO_PHYS(x)
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define MY_MALLOC 0
#define MY_SATA 1
#define SATADEBUG 
/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
static int fault_timeout;
static int sata_match (struct device *, void *, void *);
static void sata_attach (struct device *, struct device *, void *);
#define myudelay delay

int curr_device = -1;
//block_dev_desc_t sata_dev_desc[CFG_SATA_MAX_DEVICE];

/* atapi devide isatapi = 1, or isatapi = 0 
 * 0 : atapi
 * 1 : dev
 */
struct atapit devinf[2] = {
	{-1, -1},
	{-1, -1}
};
int curdev = -1;

static inline void mdelay(u64 msec)
{
	u64 i;
	for (i = 0; i < msec; i++)
		myudelay(1000);
}

static inline void sdelay(u64 sec)
{
	u64 i;
	for (i = 0; i < sec; i++)
		mdelay(1000);
}
static inline void sync(void)
{
}

#if 1
u32 strnlen(const char* s, u32 count)
{
	const char *sc;
	for(sc = s;count -- && *sc != '\0';++sc)
	{
	}
	return (sc -s);
}
u64 ata_id_n_sectors(u16 *id)
{
	if (ata_id_has_lba(id)) {
		if (ata_id_has_lba48(id))
			return ata_id_u64(id, ATA_ID_LBA48_SECTORS);
		else
			return ata_id_u32(id, ATA_ID_LBA_SECTORS);
	} else {
		return 0;
	}
}

u32 ata_dev_classify(u32 sig)
{
	u8 lbam, lbah;

	lbam = (sig >> 16) & 0xff;
	lbah = (sig >> 24) & 0xff;

	if (((lbam == 0) && (lbah == 0)) ||
			((lbam == 0x3c) && (lbah == 0xc3)))
		return ATA_DEV_ATA;

	if ((lbam == 0x14) && (lbah == 0xeb))
		return ATA_DEV_ATAPI;

	if ((lbam == 0x69) && (lbah == 0x96))
		return ATA_DEV_PMP;

	return ATA_DEV_UNKNOWN;
}

void ata_id_string(const u16 *id, unsigned char *s, unsigned int ofs, unsigned int len)
{
	unsigned int c;

	while (len > 0) {
		c = id[ofs] >> 8;
		*s = c;
		s++;

		c = id[ofs] & 0xff;
		*s = c;
		s++;

		ofs++;
		len -= 2;
	}
}

void ata_id_c_string(const u16 *id, unsigned char *s,unsigned int ofs, unsigned int len)
{
	unsigned char *p;

	ata_id_string(id, s, ofs, len - 1);

	p = s + strnlen((char *)s, len - 1);
	while (p > s && p[-1] == ' ')
		p--;
	*p = '\0';
}

void ata_dump_id(u16 *id)
{
	unsigned char serial[ATA_ID_SERNO_LEN + 1];
	unsigned char firmware[ATA_ID_FW_REV_LEN + 1];
	unsigned char product[ATA_ID_PROD_LEN + 1];
	u64 n_sectors;

	/* Serial number */
	ata_id_c_string(id, serial, ATA_ID_SERNO, sizeof(serial));
	printf("S/N: %s\n\r", serial);

	/* Firmware version */
	ata_id_c_string(id, firmware, ATA_ID_FW_REV, sizeof(firmware));
	printf("Firmware version: %s\n\r", firmware);

	/* Product model */
	ata_id_c_string(id, product, ATA_ID_PROD, sizeof(product));
	printf("Product model number: %s\n\r", product);

	/* Total sectors of device  */
	n_sectors = ata_id_n_sectors(id);
	printf("Capablity: %d sectors\n\r", n_sectors);

	printf ("id[49]: capabilities = 0x%04x\n"
			"id[53]: field valid = 0x%04x\n"
			"id[63]: mwdma = 0x%04x\n"
			"id[64]: pio = 0x%04x\n"
			"id[75]: queue depth = 0x%04x\n",
			id[49],
			id[53],
			id[63],
			id[64],
			id[75]);

	printf ("id[76]: sata capablity = 0x%04x\n"
			"id[78]: sata features supported = 0x%04x\n"
			"id[79]: sata features enable = 0x%04x\n",
			id[76],
			id[78],
			id[79]);

	printf ("id[80]: major version = 0x%04x\n"
			"id[81]: minor version = 0x%04x\n"
			"id[82]: command set supported 1 = 0x%04x\n"
			"id[83]: command set supported 2 = 0x%04x\n"
			"id[84]: command set extension = 0x%04x\n",
			id[80],
			id[81],
			id[82],
			id[83],
			id[84]);
	printf ("id[85]: command set enable 1 = 0x%04x\n"
			"id[86]: command set enable 2 = 0x%04x\n"
			"id[87]: command set default = 0x%04x\n"
			"id[88]: udma = 0x%04x\n"
			"id[93]: hardware reset result = 0x%04x\n",
			id[85],
			id[86],
			id[87],
			id[88],
			id[93]);
}

void ata_swap_buf_le16(u16 *buf, unsigned int buf_words)
{
	unsigned int i;

	for (i = 0; i < buf_words; i++)
		buf[i] = (u16)buf[i];
}

#endif

#if 0
static int ata_wait_register(volatile unsigned *addr, u32 mask,
		u32 val, u32 timeout_msec)
{
	int i;
	u32 temp;

	for (i = 0; (((temp = inl(addr)) & mask) != val)
			&& i < timeout_msec; i++)
		mdelay(1);
	return (i < timeout_msec) ? 0 : -1;
}
#endif

void dump_print(u32 reg,u32 num)
{
	u32 i;
	u32 j = 0;
	printf("\n\n%08lx to %08lx shown below\n",reg,reg + num);
	for(i = reg;i < reg + num;i+=4)
	{
		if(j % 4 == 0)
			printf("\n%08lx\t",i);
		printf("%08lx\t",inl(i));
		j ++;
	}
}
int init_sata(int dev, u32 reg)
{
	u32 length, align;
	cmd_hdr_tbl_t *cmd_hdr;
	T_ReceiveFIS *receive_fis;
	u32 cda;
	u32 i;
	u32 dma_cmdhdr_addr;
	u32 dma_receive_fis;
	atp_sata_t *sata;

	if (dev < 0 || dev > (CFG_SATA_MAX_DEVICE - 1))
	{
		printf("the sata index %d is out of ranges\n\r", dev);
		return -1;
	}
	/* Allocate SATA device driver struct */
#if MY_MALLOC
	sata = (atp_sata_t *)malloc(sizeof(atp_sata_t));
#else
	sata = (atp_sata_t *)malloc(sizeof(atp_sata_t), M_DEVBUF, M_NOWAIT);
#endif
	if (!sata)
	{
		printf("alloc the sata device struct failed\n\r");
		return -1;
	}
	/* Zero all of the device driver struct */
	memset((void *)sata, 0, sizeof(atp_sata_t));

	/* Save the private struct to block device struct */
	sata_dev_desc[dev].priv = (void *)sata;
	sprintf(sata->name, "SATA%d", dev);

	/* Set the controller register base address to device struct */
	sata->reg_base = reg;

	/* Allocate the command header table, 1024 bytes aligned */
	length = sizeof(struct cmd_hdr_tbl);
	align = SATA_HC_CMD_HDR_TBL_ALIGN;
#if MY_MALLOC
	sata->cmd_hdr_tbl_offset = (void *)malloc(length + align);
#else
	sata->cmd_hdr_tbl_offset = (void *)malloc((length + align), M_DEVBUF, M_NOWAIT);
#endif
	if (!sata->cmd_hdr_tbl_offset)
	{
		printf("alloc the command header failed\n\r");
		return -1;
	}

	cmd_hdr = (cmd_hdr_tbl_t *)(((u32)sata->cmd_hdr_tbl_offset + align) & ~(align - 1));
	sata->cmd_hdr = cmd_hdr;

	/* Zero all of the command header table */
	memset((void *)sata->cmd_hdr_tbl_offset, 0, length + align);
	pci_sync_cache(0, sata->cmd_hdr_tbl_offset, length + align, 1);

	/* Allocate receive fis */
	length = sizeof(*receive_fis);
	align = 256;
#if MY_MALLOC
	sata->receive_fis_offset = (void *)malloc(length + align);
#else
	sata->receive_fis_offset = (void *)malloc((length + align), M_DEVBUF, M_NOWAIT);
#endif
	if (!sata->receive_fis_offset)
	{
		printf("alloc the receive fis failed\n\r");
		return -1;
	}
	receive_fis = (T_ReceiveFIS*)(((u32)sata->receive_fis_offset + align) & ~(align - 1));
	sata->receive_fis = receive_fis;
	/* Zero all of receive fis */
	memset((void *)sata->receive_fis_offset, 0, length + align);
	pci_sync_cache(0,sata->receive_fis_offset, length + align, 0);

	/* Allocate command descriptor for all command */
	length = sizeof(struct cmd_desc) * SATA_HC_MAX_CMD;
	align = SATA_HC_CMD_DESC_ALIGN;
#if MY_MALLOC
	sata->cmd_desc_offset = (void *)malloc(length + align);
#else
	sata->cmd_desc_offset = (void *)malloc((length + align), M_DEVBUF, M_NOWAIT);
#endif
	if (!sata->cmd_desc_offset)
	{
		printf("alloc the command descriptor failed\n\r");
		return -1;
	}
	sata->cmd_desc = (cmd_desc_t *)(((u32)sata->cmd_desc_offset + align) & ~(align - 1));
	/* Zero all of command descriptor */
	memset((void *)sata->cmd_desc_offset, 0, length + align);
	pci_sync_cache(0, sata->cmd_desc_offset, length + align,1);
	/* Link the command descriptor to command header */
	for (i = 0; i < SATA_HC_MAX_CMD; i++)
	{
		cda = ((u32)sata->cmd_desc + SATA_HC_CMD_DESC_SIZE * i) & ~(CMD_HDR_CDA_ALIGN - 1);
		cmd_hdr->cmd_slot[i].ctba = (u32)(vtophys(cda));
	}

	/* Set the command header base address to CHBA register to tell DMA */
	dma_cmdhdr_addr = (u32)(vtophys(cmd_hdr & ~0x3ff));
	outl(reg, dma_cmdhdr_addr);
	outl(reg + 0x0004,0);

	/*Set the CFIS to chip*/
	dma_receive_fis = (u32)(vtophys(receive_fis & ~0xff));
	outl(reg + 0x0008,dma_receive_fis);
	outl(reg + 0x000c,0);

	/*Initialize the SATA channel*/
	outb(reg + 0x002c,0x01);	//send COMRESET
	mdelay(1);
	outb(reg + 0x002c,0x00);
	outb(reg + 0x0074,0x04);
	outb(reg + 0x0018,0x17);
	outl(reg + 0x0014,0);
	outl(reg + 0x0030,0xffffffff);

	return 0;
}

#define ATA_CMD_PACKET_ATAPI 0xa0
static int atp_atapi_exec_packet_cmd1(struct atp_sata *sata, struct cfis *cfis, u8 *atapi_op, int is_ncq, int tag, u8 *buffer, u32 len)
{
	cmd_hdr_entry_t *cmd_hdr;
	cmd_desc_t *cmd_desc;
	sata_fis_h2d_t *h2d;
	prd_entry_t *prde;
	u8 *cmd_atapi;
	u32 dbc;
	u32 prde_count;
	u32 buffer_length = len;
	u32 reg = sata->reg_base;
	u32 ci_map;
	u8  status;
	u8* dma_buffer;
	int is_write;
	int i;

	dma_buffer = (u8 *)vtophys((u32)buffer);
	/* Check xfer length */
	if (len > SATA_HC_MAX_XFER_LEN)
	{
		printf("max transfer length is 64MB\n\r");
		return -1;
	}

	/* Setup the command descriptor */
	cmd_desc = sata->cmd_desc + tag;

	/* Get the pointer cfis of command descriptor */
	h2d = (sata_fis_h2d_t *)cmd_desc->cfis;

	/* Zero the cfis of command descriptor */
	memset((void *)h2d, 0, 64);

	/* Get the pointer atapi of command descriptor */
	cmd_atapi = (sata_fis_h2d_t *)cmd_desc->AtapiCommand;

	/* Zero the atapi of command descriptor */
	memset((void *)h2d, 0, 32);

	/* Copy the cfis from user to command descriptor */
	h2d->fis_type = cfis->fis_type;
	h2d->FIS_Flag = cfis->FIS_Flag;
	h2d->pm_port_c = cfis->pm_port_c;

	h2d->features = cfis->features;
	h2d->features_exp = cfis->features_exp;

	h2d->lba_low = cfis->lba_low;
	h2d->lba_mid = cfis->lba_mid;
	h2d->lba_high = cfis->lba_high;
	h2d->lba_low_exp = cfis->lba_low_exp;
	h2d->lba_mid_exp = cfis->lba_mid_exp;
	h2d->lba_high_exp = cfis->lba_high_exp;
	h2d->device = cfis->device;

	if (!is_ncq)
	{
		h2d->sector_count = cfis->sector_count;
		h2d->sector_count_exp = cfis->sector_count_exp;
	} else {	 /* NCQ */
		h2d->sector_count = (u8)(tag << 3);
	}
	h2d->command = 0xA0;

	/* Acmd fillfull */
	if (atapi_op == NULL){
		memset((void *)cmd_atapi, 0, 16);
	} else{
		for (i = 0; i < 16; i++){
			cmd_atapi[i] = atapi_op[i];
		}
	}

	/* Setup the PRD table */
	prde = (prd_entry_t *)cmd_desc->prdt;
	memset((void *)prde, 0, sizeof(prd_entry_t) * 24);
	prde_count = 0;
	for (i = 0; i < SATA_HC_MAX_PRD_DIRECT; i++)
	{
		if (!len){
			break;
		}
		prde->dba_u = 0;
		if (len < PRD_ENTRY_MAX_XFER_SZ)
		{
			dbc = 0x80000000 | len;
			dbc = len;
			prde->dba = (u32)dma_buffer;
			prde->dbc = (u32)(dbc);
			prde_count++;
			prde++;
			len=0;
			break;
		} else
		{
			dbc = PRD_ENTRY_MAX_XFER_SZ; /* 4M bytes */
			prde->dba = (u32)dma_buffer;
			prde->dbc = (u32)(dbc);
			dma_buffer += PRD_ENTRY_MAX_XFER_SZ;
			len -= PRD_ENTRY_MAX_XFER_SZ;
			prde_count++;
			prde++;
		}
	}

	if(len)
		printf("left %d\n",len);

	/* is_write equal 1 identify write to cdrom;
	   is_write equal 0 identify read from cdrom */
	if(cfis->command == 0x2A)
		is_write = 1;
	else if(cmd_atapi[0] == 0x28)
		is_write = 0;
	/* Setup the command slot of cmd hdr */
	cmd_hdr = (cmd_hdr_entry_t *)&sata->cmd_hdr->cmd_slot[tag];
	memset((void*)cmd_hdr,0,0x20);

	cmd_hdr->ctba = (u32)(vtophys((u32)cmd_desc));
	cmd_hdr->ctba_u = 0;

	cmd_hdr->CommandFISLen = 5;
	cmd_hdr->AtapiPioFIS = 1;
	cmd_hdr->PreFetchAble = 0;
	cmd_hdr->Reset = 0;
	cmd_hdr->Bist = 0;
	cmd_hdr->ClearBusy = 0;
	cmd_hdr->Reserved0 = 0;
	cmd_hdr->PortMultiplier = 0;	//not sure
	cmd_hdr->prdtl = 0;
	cmd_hdr->prdbc = 0;
	cmd_hdr->ReadWrite = is_write;

	/* Make sure cmd desc and cmd slot valid before commmand issue */
	sync();

	pci_sync_cache((void *)0, (vm_offset_t *)cmd_hdr, sizeof(*cmd_hdr), 1);
	pci_sync_cache((void *)0,(vm_offset_t *)cmd_desc, sizeof(*cmd_desc), 1);
	pci_sync_cache((void *)0, (vm_offset_t *)sata->receive_fis, sizeof(*sata->receive_fis), 0);
	if(buffer && buffer_length)
		pci_sync_cache((void *)0, (vm_offset_t *)buffer, (size_t)buffer_length, is_write);

	status = inb(reg + 0x0074) & 0xFE;
	ci_map = 0x00000001;
	ci_map <<= tag;

	if (is_ncq)
	{
		status |= 0x01; 	// enable NCQ bit
		outb(reg + 0x0074,status);
		outl(reg + 0x0034,ci_map);
	}
	else
	{
		outb(reg + 0x0074,status);
	}

	//outb(reg +0x0074, inb(reg +0x0074)&0xfe);
	outl(reg + 0x0038,ci_map);

	for(i = 0;i < 100000;i ++)
	{
		if((inl(reg + 0x0038) & ci_map) == 0)
		{
			break;
		}
		myudelay(2000);
	}
	if(i >= 100000)
	{
		u32 r18;

		fault_timeout ++;
		printf("\n0x0038 not cleared,time out\n");

		r18 = inl(reg + 0x18);

		r18 &= 0xfffffffe;
		outl(reg + 0x18, r18);
		myudelay(2000);
		outl(reg + 0x18, r18 | 0x1);

		return -1;
	}

	while(inl(reg + 0x0010)==0xffffffff)
	{
		myudelay(100);
	}
	for(i = 0;i < 1000;i ++)
	{
		if(inb(reg + 0x0010) & 0x03)
		{
			break;
		}
		myudelay(1000);
	}
	if(i >= 1000)
	{
		printf("\nreceive no rfis,time out\n");
		return -1;
	}

	return len;
}

static int atp_ata_exec_ata_cmd(struct atp_sata *sata, struct cfis *cfis,
		int is_ncq, int tag, u8 *buffer, u32 len)
{
	cmd_hdr_entry_t *cmd_hdr;
	cmd_desc_t *cmd_desc;
	sata_fis_h2d_t *h2d;
	prd_entry_t *prde;
	u32 dbc;
	u32 prde_count;
	u32 buffer_length = len;
	u32 reg = sata->reg_base;
	u32 ci_map;
	u8  status;
	u8* dma_buffer;
	int is_write;
	int i;
	dma_buffer = (u8 *)vtophys((u32)buffer);
	/* Check xfer length */
	if (len > SATA_HC_MAX_XFER_LEN)
	{
		printf("max transfer length is 64MB\n\r");
		return 0;
	}

	/* Setup the command descriptor */
	cmd_desc = sata->cmd_desc + tag;

	/* Get the pointer cfis of command descriptor */
	h2d = (sata_fis_h2d_t *)cmd_desc->cfis;

	/* Zero the cfis of command descriptor */
	memset((void *)h2d, 0, 64);

	/* Copy the cfis from user to command descriptor */
	h2d->fis_type = cfis->fis_type;
	h2d->pm_port_c = cfis->pm_port_c;
	h2d->command = cfis->command;

	h2d->features = cfis->features;
	h2d->features_exp = cfis->features_exp;

	h2d->lba_low = cfis->lba_low;
	h2d->lba_mid = cfis->lba_mid;
	h2d->lba_high = cfis->lba_high;
	h2d->lba_low_exp = cfis->lba_low_exp;
	h2d->lba_mid_exp = cfis->lba_mid_exp;
	h2d->lba_high_exp = cfis->lba_high_exp;

	if (!is_ncq)
	{
		h2d->sector_count = cfis->sector_count;
		h2d->sector_count_exp = cfis->sector_count_exp;
	} else 
	{	 /* NCQ */
		h2d->sector_count = (u8)(tag << 3);
	}

	h2d->device = cfis->device;
	h2d->control = cfis->control;
	h2d->FIS_Flag = 1;

	/* Setup the PRD table */
	prde = (prd_entry_t *)cmd_desc->prdt;
	memset((void *)prde, 0, sizeof(prd_entry_t) * 24);
	prde_count = 0;
	for (i = 0; i < SATA_HC_MAX_PRD_DIRECT; i++)
	{
		if (!len)
			break;
		prde->dba_u = 0;
		if (len < PRD_ENTRY_MAX_XFER_SZ)
		{
			dbc = 0x80000000 | len;
			dbc = len;
			prde->dba = (u32)dma_buffer;
			prde->dbc = (u32)(dbc);
			prde_count++;
			prde++;
			len=0;
			break;
		} else
		{
			dbc = PRD_ENTRY_MAX_XFER_SZ; /* 4M bytes */
			prde->dba = (u32)dma_buffer;
			prde->dbc = (u32)(dbc);
			dma_buffer += PRD_ENTRY_MAX_XFER_SZ;
			len -= PRD_ENTRY_MAX_XFER_SZ;
			prde_count++;
			prde++;
		}
	}

	if(len)
		printf("left %d\n",len);

	if(cfis->command == 0x61 || cfis->command == 0x35)
		is_write = 1;
	else if(cfis->command == 0x60 || cfis->command == 0x25)
		is_write = 0;
	/* Setup the command slot of cmd hdr */
	cmd_hdr = (cmd_hdr_entry_t *)&sata->cmd_hdr->cmd_slot[tag];
	memset((void*)cmd_hdr,0,0x20);
	cmd_hdr->ctba = (u32)(vtophys((u32)cmd_desc));
	cmd_hdr->ctba_u = 0;
	cmd_hdr->CommandFISLen = 5;
	cmd_hdr->AtapiPioFIS = 0;  
	cmd_hdr->PreFetchAble = 0;
	cmd_hdr->Reset = 0;
	cmd_hdr->Bist = 0;
	cmd_hdr->ClearBusy = 0;
	cmd_hdr->Reserved0 = 0;
	cmd_hdr->PortMultiplier = 0;	//not sure
	cmd_hdr->prdtl = 0;
	cmd_hdr->prdbc = 0;
	cmd_hdr->ReadWrite = is_write;

	/* Make sure cmd desc and cmd slot valid before commmand issue */
	sync();

	status = inb(reg + 0x0074) & 0xFE;
	ci_map = 0x00000001;
	ci_map <<= tag;

	if (is_ncq)
	{
		status |= 0x01; 	// enable NCQ bit
		outb(reg + 0x0074,status);
		outl(reg + 0x0034,ci_map);
	}
	else
	{
		outb(reg + 0x0074,status);
	}
	pci_sync_cache(0,cmd_hdr,sizeof(*cmd_hdr),1);
	pci_sync_cache(0,cmd_desc,sizeof(*cmd_desc),1);
	pci_sync_cache(0,sata->receive_fis,sizeof(*sata->receive_fis),0);
	if(buffer && buffer_length)
		pci_sync_cache(0,buffer,buffer_length,is_write);
	outb(reg +0x0074,inb(reg +0x0074)&0xfe);
	outl(reg + 0x0038,ci_map);
	/* Wait command completed for 1s */
	/*	if (ata_wait_register(reg + 0x0038, ci_map, ci_map, 1000))
		{
		if (!is_ncq)
		printf("\n\rNon-NCQ command time out\n\r");
		else
		printf("\n\rNCQ command time out\n\r");
		}
	 */
	for(i = 0;i < 1000;i ++)
	{
		if((inl(reg + 0x0038) & ci_map) == 0)
		{
			break;
		}
		//zgj myudelay(1000);
		myudelay(2000);
	}
	if(i >= 1000)
	{
		fault_timeout ++;
		printf("\n0x0038 not cleared,time out\n");
		return -2;
	}
	while(inl(reg + 0x0010)==0xffffffff)
	{
		myudelay(100);
	}
	for(i = 0;i < 1000;i ++)
	{
		if(inb(reg + 0x0010) & 0x03)
		{
			break;
		}
		myudelay(1000);
	}
	if(i >= 1000)
	{
		printf("\nreceive no rfis,time out\n");
		return -1;
	}

	return len;
}

static int atp_ata_exec_reset_cmd(struct atp_sata *sata, struct cfis *cfis,
		int tag, u8 *buffer, u32 len)
{
	return 0;
}

static int cdrom_eject(int dev){
	atp_sata_t *sata = (atp_sata_t *)sata_dev_desc[dev].priv;
	struct cfis *cfist;
	struct sata_fis_h2d h2d;
	u8 *acmd;
	u8 *buffer;

	/* init CFIS structure is the item of Command Table */
	cfist = (struct cfis*)&h2d;
	memset((void *)cfist, 0, sizeof(struct cfis) );

	/* ACMD structure initialization */
	acmd = (u8 *)malloc(16, M_DEVBUF, M_NOWAIT);
	memset((void *)acmd, 0, 16);

	cfist->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	cfist->FIS_Flag = 1;
	cfist->pm_port_c = 0x0;

	cfist->features = 0;
	cfist->features_exp = 0;
	cfist->sector_count = 0;
	cfist->sector_count_exp = 0;
	cfist->lba_low = 0;
	cfist->lba_mid = 0xff;
	cfist->lba_high= 0xff;
	cfist->lba_low_exp = 0;
	cfist->lba_mid_exp = 0;
	cfist->lba_high_exp = 0;
	cfist->device = 0xe0;
	cfist->command = ATA_CMD_PACKET_ATAPI;


	buffer = (u8 *)malloc(ISO_BLOCK_SIZE, M_DEVBUF, M_NOWAIT);
	memset((void *)buffer, 0, ISO_BLOCK_SIZE);

	acmd[0] = 0x3;
	acmd[4] = 0x60;
	atp_sata_exec_cmd(sata, cfist, acmd, CMD_ATAPI, 0, buffer, ISO_BLOCK_SIZE);

	acmd[0] = 0x1b;
	acmd[4] = 0x2;
	atp_sata_exec_cmd(sata, cfist, acmd, CMD_ATAPI, 0, NULL, 0);

	acmd[0] = 0x1e;
	acmd[4] = 0x0;
	atp_sata_exec_cmd(sata, cfist, acmd, CMD_ATAPI, 0, NULL, 0);

	return 0;
}

static int atp_sata_exec_cmd(struct atp_sata *sata, struct cfis *cfis, u8 *atapicmd,
		enum cmd_type command_type, int tag, u8 *buffer, u32 len)
{
	int rc;

	if (tag > SATA_HC_MAX_CMD || tag < 0) 
	{
		printf("tag is out of range, tag=\n\r", tag);
		return -1;
	}

	switch (command_type)
	{
		case CMD_ATA:
			rc = atp_ata_exec_ata_cmd(sata, cfis, 0, tag, buffer, len);
			return rc;
		case CMD_RESET:
			rc = atp_ata_exec_reset_cmd(sata, cfis, tag, buffer, len);
			return rc;
		case CMD_NCQ:
			rc = atp_ata_exec_ata_cmd(sata, cfis, 1, tag, buffer, len);
			return rc;
		case CMD_ATAPI:
			rc = atp_atapi_exec_packet_cmd1(sata, cfis, atapicmd, 0, tag, buffer, len);
			return rc;
		case CMD_VENDOR_BIST:
		case CMD_BIST:
			printf("not support now\n\r");
			return -1;
		default:
			break;
	}

	return -1;
}


static void atp_sata_identify(int dev, u16 *id,int tag, u8 cmdop)
{
	atp_sata_t *sata = (atp_sata_t *)sata_dev_desc[dev].priv;
	cmd_hdr_entry_t *cmd_hdr;
	cmd_desc_t *cmd_desc;
	prd_entry_t *prde;
	u32 ci_map;
	u8 status74;
	u8 Error_status;
	u32 dma_buffer;
	int i;

	status74 = inb(sata->reg_base + 0x0074);
	cmd_hdr = (cmd_hdr_entry_t *)&sata->cmd_hdr->cmd_slot[tag];
	dma_buffer = vtophys((u32)id);
	cmd_desc = sata->cmd_desc + tag;

	((cfis_t*)(cmd_desc->cfis))->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	((cfis_t*)(cmd_desc->cfis))->FIS_Flag = 1;
	((cfis_t*)(cmd_desc->cfis))->pm_port_c = 0x0;

	/* lwg modify to test */
	//((cfis_t*)(cmd_desc->cfis))->device = 0xe0;
	((cfis_t*)(cmd_desc->cfis))->device = 0xe0 | (dev << 4);

	((cfis_t*)(cmd_desc->cfis))->command = cmdop;

	//Build PTD Table
	prde = (prd_entry_t*)cmd_desc->prdt;
	memset((void*)prde,0,sizeof(prd_entry_t));
	prde->dba = vtophys((u32)id);
	prde->dba_u = 0;
	prde->dbc = 0x80000200;

	cmd_hdr->PortMultiplier = 0x0;
	cmd_hdr->CommandFISLen = 0x05;
	cmd_hdr->prdtl = 0;
	cmd_hdr->prdbc = 0;

	ci_map = 0x00000001 << tag;

	pci_sync_cache(0,cmd_hdr,sizeof(*cmd_hdr),1);
	pci_sync_cache(0,cmd_desc,sizeof(*cmd_desc),1);
	pci_sync_cache(0,sata->receive_fis,sizeof(*sata->receive_fis),0);
	pci_sync_cache(0,id,ATA_ID_WORDS * 2, 0);
	outl(sata->reg_base + 0x0038,ci_map);
	for (i = 0 ; i < 1000 ; i++)
	{
		// wait CI bit clear
		if ((inl(sata->reg_base + 0x0038) & ci_map) == 0)
		{
			// CI bit to be clear
			break;
		}
		myudelay(1000);
	}
	if (i >= 1000)
	{
		fault_timeout ++;
		printf("\n0x0038 not cleared,time out\n");
		// time out
		outb(sata->reg_base + 0x0018,0x16);
		outb(sata->reg_base + 0x0018,0x17);
		outb(sata->reg_base + 0x0074,status74); // restore Reg74
		return;
	}
	for (i = 0 ; i < 1000 ; i++)
	{
		//wait 1s for D2H/PIO FIS
		if (inb(sata->reg_base + 0x0010) & 0x03)
		{
			// got D2H register FIS/ got PIO setup FIS
			break;
		}
		myudelay(1000);
	}
	if (i >= 1000)
	{
		printf("\nreceive no rfis,time out\n");
		// time out
		outb(sata->reg_base + 0x0018,0x16);
		outb(sata->reg_base + 0x0018,0x17);
		outb(sata->reg_base + 0x0074,status74); // restore Reg74
		return;
	}
	Error_status = sata->receive_fis->Device2HostFIS.Status;
	outb(sata->reg_base + 0x0010,0x03); // clear interrupt
	if (Error_status & 0x01)
	{
		// Identify command error
		outb(sata->reg_base + 0x0074,status74); // restore Reg74
		return;
	}
}

#ifdef NO_DEBUG
#define printf /\
/
#endif

static void atp_sata_set_features(int dev)
{
	atp_sata_t *sata = (atp_sata_t *)sata_dev_desc[dev].priv;
	struct sata_fis_h2d h2d;
	struct cfis *cfis;
	u8 udma_cap;

	printf("%s, dev = %x\n", __func__, dev);

	cfis = (struct cfis *)&h2d;
	memset((void *)cfis, 0, sizeof(struct cfis));

	cfis->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	cfis->pm_port_c = 0x0; /* is command */

	/* lwg modify to test */
	//cfis->device = 0xe0;
	cfis->device = 0xe0 | (dev << 4);

	cfis->command = ATA_CMD_SET_FEATURES;
	cfis->features = SETFEATURES_XFER;

	/* First check the device capablity */
	udma_cap = (u8)(sata->udma & 0xff);
	debug("udma_cap %02x\n\r", udma_cap);

	if (udma_cap == ATA_UDMA6)
		cfis->sector_count = XFER_UDMA_6;
	if (udma_cap == ATA_UDMA5)
		cfis->sector_count = XFER_UDMA_5;
	if (udma_cap == ATA_UDMA4)
		cfis->sector_count = XFER_UDMA_4;
	if (udma_cap == ATA_UDMA3)
		cfis->sector_count = XFER_UDMA_3;
	if (udma_cap == ATA_UDMA2)
		cfis->sector_count = XFER_UDMA_2;
	if (udma_cap == ATA_UDMA1)
		cfis->sector_count = XFER_UDMA_1;
	if (udma_cap == ATA_UDMA0)
		cfis->sector_count = XFER_UDMA_0;

	if (devinf[dev].isatapi == 1)
		cfis->sector_count = 0x42;

	atp_sata_exec_cmd(sata, cfis, NULL, CMD_ATA, 0, NULL, 0);
}


#ifdef NO_DEBUG
#undef printf 
#endif

static u32 atp_sata_rw_cmd(int dev, u32 start, u32 blkcnt, u8 *buffer, int is_write)
{
	atp_sata_t *sata = (atp_sata_t *)sata_dev_desc[dev].priv;
	struct sata_fis_h2d h2d;
	struct cfis *cfis;
	u32 block;

	block = start;
	cfis = (struct cfis *)&h2d;

	memset((void *)cfis, 0, sizeof(struct cfis));

	cfis->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	cfis->pm_port_c = 0x80; /* is command */
	cfis->command = (is_write) ? ATA_CMD_WRITE : ATA_CMD_READ;
	cfis->device = ATA_LBA;

	cfis->device |= (block >> 24) & 0xf;
	cfis->lba_high = (block >> 16) & 0xff;
	cfis->lba_mid = (block >> 8) & 0xff;
	cfis->lba_low = block & 0xff;
	cfis->sector_count = (u8)(blkcnt & 0xff);

	atp_sata_exec_cmd(sata, cfis, NULL, CMD_ATA, 0, buffer, ATA_SECT_SIZE * blkcnt);
	return blkcnt;
}

/* atapi lock/unlock media
 * islock : 1 -- lock
 0 -- unlock
 */
int atp_sata_lock_atapi_cmd(int dev, int islock){
	atp_sata_t *sata = (atp_sata_t *)sata_dev_desc[dev].priv;
	struct sata_fis_h2d h2d;
	struct cfis *cfist;
	u8 *acmd;
	u8 *buffer;
	int rc;

	buffer = (u8 *)malloc(ISO_BLOCK_SIZE * 12, M_DEVBUF, M_NOWAIT);
	memset((void *)buffer, 0xee, ISO_BLOCK_SIZE*12);

	cfist = (struct cfis *)&h2d;
	memset((void *)cfist, 0, sizeof(struct cfis));

	acmd = (u8 *)malloc(16, M_DEVBUF, M_NOWAIT);
	memset((void *)acmd, 0, 16);

	/* ACMD structure initialization */
	acmd = (u8 *)malloc(16, M_DEVBUF, M_NOWAIT);
	memset((void *)acmd, 0, 16);

	cfist->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	cfist->FIS_Flag = 1;
	cfist->pm_port_c = 0x0;

	cfist->features = 0;
	cfist->features_exp = 0;
	cfist->sector_count = 0;
	cfist->sector_count_exp = 0;
	cfist->lba_low = 0;
	cfist->lba_mid = 0xff;
	cfist->lba_high= 0xff;
	cfist->lba_low_exp = 0;
	cfist->lba_mid_exp = 0;
	cfist->lba_high_exp = 0;
	
	/* lwg modify to test */
	cfist->device = 0xe0 | (dev << 4);
	cfist->command = ATA_CMD_PACKET_ATAPI;

	acmd[0] = 0x1e;
	if (islock == 1)  //lock media
		acmd[4] = 0x4;
	else if (islock == 0) //unlock media
		acmd[4] = 0x0;

	rc = atp_sata_exec_cmd(sata, cfist, acmd, CMD_ATAPI, 0, buffer, ISO_BLOCK_SIZE);

	return rc;
}

/* require sense
 * command comde 0x03
 */
int atp_sata_rqsense_atapi_cmd(int dev){
	atp_sata_t *sata = (atp_sata_t *)sata_dev_desc[dev].priv;
	struct sata_fis_h2d h2d;
	struct cfis *cfist;
	u8 *acmd;
	u8 *buffer;
	int rc;

	buffer = (u8 *)malloc(ISO_BLOCK_SIZE * 12, M_DEVBUF, M_NOWAIT);
	memset((void *)buffer, 0xee, ISO_BLOCK_SIZE * 12);

	cfist = (struct cfis *)&h2d;
	memset((void *)cfist, 0, sizeof(struct cfis));

	acmd = (u8 *)malloc(16, M_DEVBUF, M_NOWAIT);
	memset((void *)acmd, 0, 16);

	/* ACMD structure initialization */
	acmd = (u8 *)malloc(16, M_DEVBUF, M_NOWAIT);
	memset((void *)acmd, 0, 16);

	cfist->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	cfist->FIS_Flag = 1;
	cfist->pm_port_c = 0x0;
	cfist->features = 1;
	cfist->features_exp = 0;
	cfist->sector_count = 0;
	cfist->sector_count_exp = 0;
	cfist->lba_low = 0;
	cfist->lba_mid = 0xff;
	cfist->lba_high= 0xff;
	cfist->lba_low_exp = 0;
	cfist->lba_mid_exp = 0;
	cfist->lba_high_exp = 0;

	/* lwg modify to test */
	//cfist->device = 0xe0;
	cfist->device = 0xe0 | (dev << 4);

	cfist->command = ATA_CMD_PACKET_ATAPI;

	acmd[0] = 0x3;
	acmd[4] = 0x60;
	rc = atp_sata_exec_cmd(sata, cfist, acmd, CMD_ATAPI, 0, buffer, ISO_BLOCK_SIZE);

	return rc;
}

/* atapi read/write function by lwg*/
u32 atp_sata_rw_atapi_cmd(int dev, u32 start, u32 blkcnt, u8  *buffer, int is_write){
	atp_sata_t *sata = (atp_sata_t *)sata_dev_desc[dev].priv;
	struct sata_fis_h2d h2d;
	struct cfis *cfist;
	u8 *acmd;

	cfist = (struct cfis *)&h2d;
	memset((void *)cfist, 0, sizeof(struct cfis));

	acmd = (u8 *)malloc(16, M_DEVBUF, M_NOWAIT);
	memset((void *)acmd, 0, 16);

	/* ACMD structure initialization */
	acmd = (u8 *)malloc(16, M_DEVBUF, M_NOWAIT);
	memset((void *)acmd, 0, 16);

	cfist->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	cfist->FIS_Flag = 1;
	cfist->pm_port_c = 0x0;
	cfist->features = 1;
	cfist->features_exp = 0;
	cfist->sector_count = 0;
	cfist->sector_count_exp = 0;
	cfist->lba_low = 0;
	cfist->lba_mid = 0xff;
	cfist->lba_high= 0xff;
	cfist->lba_low_exp = 0;
	cfist->lba_mid_exp = 0;
	cfist->lba_high_exp = 0;

	/* lwg modify to test */
	//cfist->device = 0xe0;
	cfist->device = 0xe0 | (dev<<4);

	cfist->command = ATA_CMD_PACKET_ATAPI;

	acmd[0] = 0x28;
	acmd[5] = start & 0xff;
	acmd[4] = (start >> 8) & 0xff;
	acmd[3] = (start >> 16) & 0xff;
	acmd[2] = (start >> 24) & 0xff;

	acmd[8] = (u8)(blkcnt & 0xff);
	acmd[7] = (u8)((blkcnt >> 8) & 0xff);

	atp_sata_exec_cmd(sata, cfist, acmd, CMD_ATAPI, 0, buffer, ISO_BLOCK_SIZE * blkcnt);

	return blkcnt;
}

static u32 atp_sata_rw_cmd_ext(int dev, u32 start, u32 blkcnt, u8 *buffer, int is_write)
{
	atp_sata_t *sata = (atp_sata_t *)sata_dev_desc[dev].priv;
	struct sata_fis_h2d h2d;
	struct cfis *cfis;
	u64 block;

	block = (u64)start;
	cfis = (struct cfis *)&h2d;

	memset((void *)cfis, 0, sizeof(struct cfis));

	cfis->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	cfis->pm_port_c = 0x80; /* is command */

	cfis->command = (is_write) ? ATA_CMD_WRITE_EXT
		: ATA_CMD_READ_EXT;

	cfis->lba_high_exp = (block >> 40) & 0xff;
	cfis->lba_mid_exp = (block >> 32) & 0xff;
	cfis->lba_low_exp = (block >> 24) & 0xff;
	cfis->lba_high = (block >> 16) & 0xff;
	cfis->lba_mid = (block >> 8) & 0xff;
	cfis->lba_low = block & 0xff;
	cfis->device = 0xe0;
	cfis->sector_count_exp = (blkcnt >> 8) & 0xff;
	cfis->sector_count = blkcnt & 0xff;

	atp_sata_exec_cmd(sata, cfis, NULL, CMD_ATA, 0, buffer, ATA_SECT_SIZE * blkcnt);

	return blkcnt;
}

u32 atp_sata_rw_ncq_cmd(int dev, u32 start, u32 blkcnt, u8 *buffer, int is_write)
{
	atp_sata_t *sata = (atp_sata_t *)sata_dev_desc[dev].priv;
	struct sata_fis_h2d h2d;
	struct cfis *cfis;
	int ncq_channel;
	u64 block;

	if (sata_dev_desc[dev].lba48 != 1)
	{
		printf("execute FPDMA command on non-LBA48 hard disk\n\r");
		return -1;
	}

	block = (u64)start;
	cfis = (struct cfis *)&h2d;

	memset((void *)cfis, 0, sizeof(struct cfis));

	cfis->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	cfis->pm_port_c = 0x80; /* is command */

	cfis->command = (is_write) ? ATA_CMD_FPDMA_WRITE
		: ATA_CMD_FPDMA_READ;

	cfis->lba_high_exp = (block >> 40) & 0xff;
	cfis->lba_mid_exp = (block >> 32) & 0xff;
	cfis->lba_low_exp = (block >> 24) & 0xff;
	cfis->lba_high = (block >> 16) & 0xff;
	cfis->lba_mid = (block >> 8) & 0xff;
	cfis->lba_low = block & 0xff;

	cfis->device = 0x40;
	cfis->features_exp = (blkcnt >> 8) & 0xff;
	cfis->features = blkcnt & 0xff;

	if (sata->queue_depth >= SATA_HC_MAX_CMD)
		ncq_channel = SATA_HC_MAX_CMD - 1;
	else
		ncq_channel = sata->queue_depth - 1;

	/* Use the latest queue */
	atp_sata_exec_cmd(sata, cfis, NULL, CMD_NCQ, ncq_channel, buffer, ATA_SECT_SIZE * blkcnt);
	return blkcnt;
}

/* Software reset, set SRST of the Device Control register */
int atp_sata_software_reset(int dev,int tag)
{

	atp_sata_t *sata = (atp_sata_t *)sata_dev_desc[dev].priv;
	cmd_hdr_entry_t *cmd_hdr;
	cmd_desc_t *cmd_desc;
	u32 ci_map;


	if((inb(sata->reg_base + 0x28) & 0x0f) != 0x03){
		return 0;
	}
	memset((void *)sata->cmd_desc, 0, 64);
	cmd_hdr = (cmd_hdr_entry_t *)&sata->cmd_hdr->cmd_slot[tag];
	cmd_desc = sata->cmd_desc + tag;
	memset((void*)cmd_hdr,0,0x400);
	//================================================================
	//	sata->cmd_hdr->cmd_slot[tag].PortMultiplier = 0x0f;
	cmd_hdr->CommandFISLen = 0x05;
	cmd_hdr->ClearBusy = 1;
	//	cmd_hdr->PortMultiplier = 0x0f;
	cmd_hdr->ctba = vtophys((u32)(cmd_desc));
	//	((cfis_t*)(cmd_desc->cfis))->pm_port_c = 0x0f;;
	((cfis_t*)(cmd_desc->cfis))->fis_type = SATA_FIS_TYPE_REGISTER_H2D;
	((cfis_t*)(cmd_desc->cfis))->FIS_Flag = 0;
	if (tag == 0)
		((cfis_t*)(cmd_desc->cfis))->control = 0x04;	//RST bit =1
	else if (tag == 1)
		((cfis_t*)(cmd_desc->cfis))->control = 0x08;

	ci_map = 0x00000001 << tag;
	SATADEBUG
		pci_sync_cache(0,cmd_hdr,sizeof(*cmd_hdr),1);
	pci_sync_cache(0,cmd_desc,sizeof(*cmd_desc),1);
	pci_sync_cache(0,sata->receive_fis,sizeof(*sata->receive_fis),0);

	//	ci_map = 0x00000001 << 7;
	outl(sata->reg_base + 0x0038 ,ci_map);
	while (inl(sata->reg_base + 0x0038) & ci_map)
	{
		myudelay(100);
	}
	myudelay(10);

	cmd_hdr->ClearBusy = 0;
	((cfis_t*)cmd_desc->cfis)->control = 0;		//RST bit = 0
	pci_sync_cache(0,cmd_hdr,sizeof(*cmd_hdr),1);
	pci_sync_cache(0,cmd_desc,sizeof(*cmd_desc),1);
	pci_sync_cache(0,sata->receive_fis,sizeof(*sata->receive_fis),0);
	outl(sata->reg_base + 0x0038,ci_map);
	while (inl(sata->reg_base + 0x0038) & ci_map)
	{
		myudelay(100);
	}
	myudelay(100);
	outb(sata->reg_base + 0x0010,inb(sata->reg_base + 0x0010));	//clear error
	SATADEBUG

		return 1;
}

u32 ata_low_level_rw_lba48(int dev, u32 blknr, u32 blkcnt, void *buffer, int is_write)
{
	u32 start, blks;
	u8 *addr;
	int max_blks;
	start = blknr;
	blks = blkcnt;
	addr = (u8 *)buffer;

	max_blks = ATA_MAX_SECTORS_LBA48;
	do {
		if (blks > max_blks)
		{
			if (curdev==dev && devinf[curdev].isatapi==0){
				atp_sata_rw_cmd_ext(dev, start, max_blks, addr, is_write);
				addr += ATA_SECT_SIZE * max_blks;
			}
			else if (curdev==dev && devinf[curdev].isatapi==1){
				atp_sata_rw_atapi_cmd(dev, start, max_blks, addr, is_write);
				addr += ISO_BLOCK_SIZE * max_blks;
			}
			start += max_blks;
			blks -= max_blks;
		} else {
			if (curdev==dev && devinf[curdev].isatapi==0){
				atp_sata_rw_cmd_ext(dev, start, blks, addr, is_write);
				addr += ATA_SECT_SIZE * blks;
			}
			else if (curdev==dev && devinf[curdev].isatapi==1){
				atp_sata_rw_atapi_cmd(dev, start, blks, addr, is_write);
				addr += ISO_BLOCK_SIZE * blks;
			}
			start += blks;
			blks = 0;
		}
	} while (blks != 0);

	return blkcnt;
}

u32 ata_low_level_rw_lba28(int dev, u32 blknr, u32 blkcnt, void *buffer, int is_write)
{
	u32 start, blks;
	u8 *addr;
	int max_blks;

	start = blknr;
	blks = blkcnt;
	addr = (u8 *)buffer;

	/* lwg  modify to compatible cdrom */
	if (curdev==dev && devinf[curdev].isatapi==1){
		max_blks = ATA_SECT_SIZE * 4;
	}
	else if (curdev==dev && devinf[curdev].isatapi==0){
		max_blks = ATA_MAX_SECTORS;
	}
	do {
		if (blks > max_blks)
		{
			if (curdev==dev && devinf[curdev].isatapi==0){
				atp_sata_rw_cmd(dev, start, max_blks, addr, is_write);
				addr += ATA_SECT_SIZE * max_blks;
			}
			else if (curdev==dev && devinf[curdev].isatapi==1){
				atp_sata_rw_atapi_cmd(dev, start, max_blks, addr, is_write);
				addr += ISO_BLOCK_SIZE * max_blks;
			}
			start += max_blks;
			blks -= max_blks;
			addr += ATA_SECT_SIZE * max_blks;
		}
		else
		{
			if (curdev==dev && devinf[curdev].isatapi==0){
				atp_sata_rw_cmd(dev, start, blks, addr, is_write);
				addr += ATA_SECT_SIZE * blks;
			}
			else if (curdev==dev && devinf[curdev].isatapi==1){
				atp_sata_rw_atapi_cmd(dev, start, blks, addr, is_write);
				addr += ISO_BLOCK_SIZE * blks;
			}
			start += blks;
			blks = 0;
		}
	} while (blks != 0);

	return blkcnt;
}

/*
 * SATA interface between low level driver and command layer
 */
ulong atp_sata_read(int dev, unsigned long blknr, lbaint_t blkcnt, void *buffer)
{
	u32 rc;

	if (sata_dev_desc[dev].lba48)
		rc = ata_low_level_rw_lba48(dev, blknr, blkcnt, buffer, READ_CMD);
	else
		rc = ata_low_level_rw_lba28(dev, blknr, blkcnt, buffer, READ_CMD);

	return rc;
}

ulong atp_sata_write(int dev, unsigned long blknr, lbaint_t blkcnt, void *buffer)
{
	u32 rc;

	if (sata_dev_desc[dev].lba48) {
		rc = ata_low_level_rw_lba48(dev, blknr, blkcnt, buffer, WRITE_CMD);
	} else {
		rc = ata_low_level_rw_lba28(dev, blknr, blkcnt, buffer, WRITE_CMD);
	}

	return rc;
}

#ifdef NO_DEBUG
#define printf /\
/
#endif

int scan_sata(int dev)
{
	atp_sata_t *sata =  (atp_sata_t *)sata_dev_desc[dev].priv;
	u8 serial[ATA_ID_SERNO_LEN + 1];
	u8 firmware[ATA_ID_FW_REV_LEN + 1];
	u8 product[ATA_ID_PROD_LEN + 1];
	u8 statusX;
	u8 status;
	u32 signature;
	u32 *id;
	u64 n_sectors;
	u32 reg =  sata->reg_base;
	int k;

	for(k = 0;k < 1000;k++) {
		statusX = (inb(reg + 0x0028) & 0x0F);
		if(statusX == 0x03) {
			break;
		}
		myudelay(1000);
	}
	printf("statusX = %d, k = %d, dev = %x\n",statusX, k, dev);

	/* Device presence detected and Phy communication established */
	if(statusX == 0x03){
		// =====================================================================
		// Physical Ready clear BUSY & DRQ bit to release channel
		// =====================================================================
		status = inb(reg + 0x0018) | 0x08;
		outb(reg + 0x0018,status);
		while(inb(reg + 0x0018) & 0x08) {
			myudelay(10);
		}
		if(atp_sata_software_reset(dev,0)) {
			for(k = 0;k < 1000;k++) {
				signature = inl(reg + 0x0024);
				/* found ATA DEVICE, ATAPI DEVICE, PORT Multiplier */
				if ((signature == 0x00000101) || (signature == 0xeb140101)
					 ||(signature == 0x96690101)) {
					break;
				}
				myudelay(1000);
			}
			id = (u32 *)malloc((ATA_ID_WORDS * 2), M_DEVBUF, M_NOWAIT);
			if (!id) {
				printf("id malloc failed\n\r");
				return -1;
			}
			memset((void*)id,0,0x200);

#if 1
			printf("*************************************************\n");
			if(signature == 0x00000101){
				/* found ata device */

				printf("SATA HARDDISK block configure -- dev = %x\n", dev);
				sata_dev_desc[dev].if_type = IF_TYPE_SATA;
				sata_dev_desc[dev].dev = dev;
				sata_dev_desc[dev].part_type = PART_TYPE_UNKNOWN;
				sata_dev_desc[dev].type = DEV_TYPE_HARDDISK;
				sata_dev_desc[dev].lba = 0;
				sata_dev_desc[dev].blksz = 512;

				devinf[dev].isatapi = 0;
				devinf[dev].devnu = dev;

				/* Identify device to get information */
				atp_sata_identify(dev, (u16*)id,0, 0xec);
				outb(reg + 0x0018,0x17);               // CLEAR BSY & DRQ
				outl(reg + 0x0014,0x00000009);         // enable P0IS.SDBS, P0IS.DHRS
				outl(reg + 0x0074,0);
				statusX = inb(reg + 0x007A) | 0x77;
				outb(reg + 0x007A,statusX);

			}else if(signature == 0xeb140101){
				/* found atapi device */

				printf("SATA CDROM block configure -- dev = %x\n", dev);
				sata_dev_desc[dev].if_type = IF_TYPE_ATAPI;
				sata_dev_desc[dev].dev = dev;
				sata_dev_desc[dev].part_type = PART_TYPE_UNKNOWN;
				sata_dev_desc[dev].type = DEV_TYPE_CDROM;
				sata_dev_desc[dev].lba = 0;
				sata_dev_desc[dev].lba48 = 1;
				sata_dev_desc[dev].blksz = ISO_BLOCK_SIZE;

				devinf[dev].isatapi = 1;
				devinf[dev].devnu = dev;

				/* Identify device to get information */
				atp_sata_identify(dev, (u16*)id, 0, 0xa1);
				outb(reg + 0x0018,0x17);               // CLEAR BSY & DRQ
				outl(reg + 0x0014,0x00000009);         // enable P0IS.SDBS, P0IS.DHRS
				outl(reg + 0x0074,0);
			}

			atp_sata_set_features(dev);

			/* Serial number */
			ata_id_c_string((u16*)id, serial, ATA_ID_SERNO, sizeof(serial));
			memcpy(sata_dev_desc[dev].product, serial, sizeof(serial));
			printf("lwg : serialNo= %s\n", serial);

			/* Firmware version */
			ata_id_c_string((u16*)id, firmware, ATA_ID_FW_REV, sizeof(firmware));
			memcpy(sata_dev_desc[dev].revision, firmware, sizeof(firmware));
			printf("lwg : revision = %s\n", firmware);

			/* Product model */
			ata_id_c_string((u16*)id, product, ATA_ID_PROD, sizeof(product));
			memcpy(sata_dev_desc[dev].vendor, product, sizeof(product));
			printf("lwg : vendor = %s\n", product);

			/* Totoal sectors */
			n_sectors = ata_id_n_sectors((u16*)id);
			sata_dev_desc[dev].lba = (u16*)n_sectors;
			//printf("lwg : n_sectors = %x\n", n_sectors);
			//printf("lwg : lba = %x\n", sata_dev_desc[dev].lba);

			/* Check if support LBA48 */
			if (ata_id_has_lba48((u16*)id)) {
				sata_dev_desc[dev].lba48 = 1;
			}
#endif 
			/*XXX Set the xfer mode to highest speed */
			free((void *)id, M_DEVBUF);;
		}
		outl(reg + 0x30, 0xffffffff);
	}else 
		return -1;

	return 0;
}

#ifdef NO_DEBUG
#undef printf
#endif

int atp_sata_initialize(u32 reg,u32 flags);

typedef struct atp_sata_softc {
	/* General disk infos */
	struct device sc_dev;
	int dev;
	int bs,count;
} atp_sata_softc;

struct cfattach sata_ca = {
	sizeof(atp_sata_softc),sata_match, sata_attach,
};


//DV_IFNET means network interface
//defined in sys/dev/device.h

struct cfdriver sata_cd = {
	NULL, "sata", DV_DISK,
};
static int sata_match(
		struct device *parent,
		void   *match,
		void * aux
		)
{
	int err;
	atp_sata_info_t *pinfo = aux;

	if (pinfo->aa_link.aa_type == 0xff && (pinfo->flags == 0 || pinfo->flags == 1))
	{
		err = atp_sata_initialize(pinfo->sata_reg_base, pinfo->flags);
		if(err)
			return 0;
		else
			return 1;
	}
	else
		return 0;
}

static void sata_attach(struct device * parent,struct device * self,void *aux)
{
	atp_sata_softc *sc = (atp_sata_softc * )self;
	atp_sata_info_t *pinfo = aux;
	sc->dev = pinfo->flags;
}



void init_part (block_dev_desc_t* dev_desc)
{

}

int atp_sata_initialize(u32 reg,u32 flags)
{
	int rc;
	int i=flags;
	u32 signature;

	memset(&sata_dev_desc[i], 0, sizeof(struct block_dev_desc));
	sata_dev_desc[i].dev = i;
	sata_dev_desc[i].block_read = atp_sata_read;
	sata_dev_desc[i].block_write = atp_sata_write;
#if 0
	SATADEBUG
		if (i == 0){
			printf("SATA CDROM block configure\n");
			memset(&sata_dev_desc[i], 0, sizeof(struct block_dev_desc));
			sata_dev_desc[i].if_type = IF_TYPE_ATAPI;
			sata_dev_desc[i].dev = i;
			sata_dev_desc[i].part_type = PART_TYPE_UNKNOWN;
			sata_dev_desc[i].type = DEV_TYPE_CDROM;
			sata_dev_desc[i].lba = 0;
			sata_dev_desc[i].lba48 = 1;
			sata_dev_desc[i].blksz = ISO_BLOCK_SIZE;
			sata_dev_desc[i].block_read = atp_sata_read;
			sata_dev_desc[i].block_write = atp_sata_write;
		}else if (i == 1){
			printf("SATA HARDDISK block configure\n");
			memset(&sata_dev_desc[i], 0, sizeof(struct block_dev_desc));
			sata_dev_desc[i].if_type = IF_TYPE_SATA;
			sata_dev_desc[i].dev = i;
			sata_dev_desc[i].part_type = PART_TYPE_UNKNOWN;
			sata_dev_desc[i].type = DEV_TYPE_HARDDISK;
			sata_dev_desc[i].lba = 0;
			sata_dev_desc[i].blksz = 512;
			sata_dev_desc[i].block_read = atp_sata_read;
			sata_dev_desc[i].block_write = atp_sata_write;
		}

	SATADEBUG
#endif
	rc = init_sata(i, reg);
	rc = scan_sata(i);
	SATADEBUG
		if ((sata_dev_desc[i].lba > 0) && (sata_dev_desc[i].blksz > 0)){
			init_part(&sata_dev_desc[i]);
		}

	curr_device = 0;
	return rc;
}

block_dev_desc_t *sata_get_dev(int dev)
{
	return (dev < CFG_SATA_MAX_DEVICE) ? &sata_dev_desc[dev] : NULL;
}

//////////////////satafs//////////////////
/*
 * Supported paths:
 *      /dev/sata
 */
#include <fcntl.h>
#include <file.h>
#include <sys/buf.h>
#include <ramfile.h>
#include <sys/unistd.h>
#undef _KERNEL
#include <errno.h>

block_dev_desc_t sata_dev_desc[CFG_SATA_MAX_DEVICE];

#define atp_sata_lookup(dev) (struct atp_sata_softc *)device_lookup(&sata_cd, minor(dev))

void atp_sata_strategy(struct buf *bp)
{
	struct atp_sata_softc *priv;
	unsigned int blkno, blkcnt;
	int ret;

	priv = atp_sata_lookup(bp->b_dev);
	blkno = bp->b_blkno;

	/* lwg modify to compatible cdrom */
	if (curdev==priv->dev && devinf[curdev].isatapi==1){
		blkcnt = howmany(bp->b_bcount, priv->bs*4);
	}
	if (curdev==priv->dev && devinf[curdev].isatapi==0){
		blkno = blkno / (priv->bs/DEV_BSIZE);
		blkcnt = howmany(bp->b_bcount, priv->bs);
	}

	/* Invalid Request?  */
	if (bp->b_blkno < 0 || (bp->b_bcount % priv->bs) != 0 ||
			(bp->b_bcount / priv->bs) >= (1 << NBBY)) {
		bp->b_error = EINVAL;
		printf("Invalid request \n");
		goto bad;
	}

	/* If it's a null transfer, return immediately. */
	if (bp->b_bcount == 0){
		goto done;
	}

	if(bp->b_flags & B_READ){
		fault_timeout = 0;
		ret=atp_sata_read(priv->dev, blkno, blkcnt, bp->b_data);

		/* lwg add to compatible cdrom */
		if (curdev==priv->dev && devinf[curdev].isatapi==1){
			fault_timeout = 0;
		}

		if(ret != blkcnt || fault_timeout){
			bp->b_flags |= B_ERROR;
		}
		dotik(30000, 0);
	}
	else
	{
		fault_timeout = 0;
		ret=atp_sata_write(priv->dev,blkno,blkcnt,bp->b_data);
		if(ret != blkcnt||fault_timeout)
			bp->b_flags |= B_ERROR;
		dotik(30000, 0);
	}

done:
	biodone(bp);
	return;
bad:
	bp->b_flags |= B_ERROR;
	biodone(bp);
}


int atp_open( dev_t dev, int flag, int fmt, struct proc *p)
{
	struct atp_sata_softc *priv;

	priv = atp_sata_lookup(dev);
	if(!priv)
		return -1;

	/* lwg add for atapi device */

	/* atapi device */
	if (devinf[priv->dev].isatapi == 1){

		atp_sata_rqsense_atapi_cmd(devinf[priv->dev].devnu);
		atp_sata_lock_atapi_cmd(devinf[priv->dev].devnu, 1);
		priv->bs= ISO_BLOCK_SIZE;

	}
	/* ata device */
	else if (devinf[priv->dev].isatapi == 0){
		priv->bs= ATA_SECT_SIZE;
	}
	priv->count=-1;
	curdev = priv->dev;

	return 0;
}


int atp_close( dev_t dev, int flag, int fmt, struct proc *p)
{
	/* lwg add for atapi device */
	if (curdev==dev && devinf[curdev].isatapi==1){
		atp_sata_lock_atapi_cmd(dev, 0);
	}
	curdev = -1;
	return 0;
}

int atp_read( dev_t dev, struct uio *uio, int flags)
{
	return physio(atp_sata_strategy, NULL, dev, B_READ, minphys, uio);
}

int atp_write( dev_t dev, struct uio *uio, int flags)
{
	return (physio(atp_sata_strategy, NULL, dev, B_WRITE, minphys, uio));
}
///////////////////satafs end//////////////////////
int cmd_sata_atp(int argc, char *argv[])
{
	int rc = 0;
	int i = 0;
	switch (argc) {
		case 0:
		case 1:
			printf("Hello Sata!!!\n");
			return 1;
		case 2:
			if (strncmp(argv[1],"inf", 3) == 0) {
				int i;
				printf("\n");
				for (i = 0; i < CFG_SATA_MAX_DEVICE; ++i) {
					if (sata_dev_desc[i].type == DEV_TYPE_UNKNOWN)
					{
						printf("sata_dev_desc[%d].type = %d\n",i,sata_dev_desc[i].type);
						continue;
					}
					printf ("SATA device %d:\n", i);
				}
				return 0;
			} else if (strncmp(argv[1],"dev", 3) == 0) {
				if ((curr_device < 0) || (curr_device >= CFG_SATA_MAX_DEVICE)) {
					printf("dev-curr_device=%d\n",curr_device);
					printf("no SATA devices available\n");
					return 1;
				}
				printf("SATA device %d:\n", curr_device);
				return 0;
			} else if (strncmp(argv[1],"part",4) == 0) {
				int dev, ok;

				for (ok = 0, dev = 0; dev < CFG_SATA_MAX_DEVICE; ++dev) {
					if (sata_dev_desc[dev].part_type != PART_TYPE_UNKNOWN) {
						++ok;
					}
				}
				if (!ok) {
					printf("\nno SATA devices available\n");
					rc ++;
				}
				return rc;
			}
			return 1;
		case 3:
			if (strncmp(argv[1], "dev", 3) == 0) {
				int dev = (int)strtoul(argv[2], NULL, 10);

				printf("SATA device %d:\n", dev);
				if (dev >= CFG_SATA_MAX_DEVICE) {
					printf ("unknown device\n");
					return 1;
				}

				if (sata_dev_desc[dev].type == DEV_TYPE_UNKNOWN)
					return 1;

				curr_device = dev;

				printf("... is now current device\n");

				return 0;
			} else if (strncmp(argv[1], "part", 4) == 0) {
				int dev = (int)strtoul(argv[2], NULL, 10);

				if (sata_dev_desc[dev].part_type != PART_TYPE_UNKNOWN) {
				} else {
					printf("\nSATA device %d not available\n", dev);
					rc = 1;
				}
				return rc;
			}
			return 1;

		default: /* at least 4 args */
			if (strcmp(argv[1], "read") == 0) {
				ulong addr = strtoul(argv[2], NULL, 16);
				ulong cnt = strtoul(argv[4], NULL, 16);
				ulong n;
				lbaint_t blk = strtoul(argv[3], NULL, 16);

				printf("\nSATA read: device %d block # %ld, count %ld ... ",
						curr_device, blk, cnt);

				n = atp_sata_read(curr_device, blk, cnt, (u32 *)addr);
				printf("the buffer address is 0x%x\n",addr);
				for(i = 0 ; i < n;)
				{
					printf("%8x",*((u32 *)addr+i));
					i++;
					if(i%8==0)
						printf("\n");

				}

				/* flush cache after read */
				flush_cache(addr, cnt * sata_dev_desc[curr_device].blksz);

				printf("n = %d,cnt = %d\n",n,cnt);
				printf("%ld blocks read: %s\n",
						n, (n==cnt) ? "OK" : "ERROR");
				return (n == cnt) ? 0 : 1;
			} else if (strcmp(argv[1], "write") == 0) {
				ulong addr = strtoul(argv[2], NULL, 16);
				ulong cnt = strtoul(argv[4], NULL, 16);
				ulong n;

				lbaint_t blk = strtoul(argv[3], NULL, 16);

				printf("\nSATA write: device %d block # %ld, count %ld ... ",
						curr_device, blk, cnt);

				n = atp_sata_write(curr_device, blk, cnt, (u32 *)addr);

				printf("%ld blocks written: %s\n",
						n, (n == cnt) ? "OK" : "ERROR");
				return (n == cnt) ? 0 : 1;
			} else {
				rc = 1;
			}

			return rc;
	}
}
/*
 *  Command table registration
 *  ==========================
 */

static const Cmd Cmds[] =
{
	{"ATPsata"},
	{"atpsata", "[info device part read write]", 0, "atp sata read write", cmd_sata_atp, 1, 99, 0},
	{0, 0}
};

static void init_cmd __P((void)) __attribute__ ((constructor));

	void
init_cmd()
{
	cmdlist_expand(Cmds, 1);
}

