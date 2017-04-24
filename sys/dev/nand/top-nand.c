
#include <machine/types.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <sys/device.h>
#include "fcr-nand.h"
/*
 * MTD structure for fcr_soc board
 */
static struct mtd_info *fcr_soc_mtd = NULL;

static void fcr_soc_hwcontrol(struct mtd_info *mtd, int dat,unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;

if((ctrl & NAND_CTRL_ALE)==NAND_CTRL_ALE)
		*(volatile unsigned char *)(0xbe000082) = dat;
if ((ctrl & NAND_CTRL_CLE)==NAND_CTRL_CLE)
		*(volatile unsigned char *)(0xbe000081) = dat;

}

static void find_good_part(struct mtd_info *fcr_soc_mtd)
{
	int offs;
	int start=-1;
	char name[20];
	int idx=1;

	for(offs=0;offs< fcr_soc_mtd->size;offs+=fcr_soc_mtd->erasesize)
	{
		if(fcr_soc_mtd->block_isbad(fcr_soc_mtd,offs)&& start>=0)
		{
			sprintf(name,"mtd%d",idx++);
			add_mtd_device(fcr_soc_mtd,start,offs-start,name);
			printf("find good part:  %s : start:0x%08x,offs:0x%08x\n",name,start,offs);
			start=-1;
		}
		else if(start<0)
		{
			start=offs;
		}
	}

	if(start>=0)
	{
		sprintf(name,"mtd%d",idx++);
		add_mtd_device(fcr_soc_mtd,start,offs-start,name);
		printf("find good part:  %s : start:0x%08x,offs:0x%08x\n",name,start,offs);
	}
}

/*
 * Main initialization routine
 */
int topstar_nand_init(void)
{
	struct nand_chip *this;

	/* Allocate memory for MTD device structure and private data */
	fcr_soc_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
	if (!fcr_soc_mtd) {
		printk("Unable to allocate fcr_soc NAND MTD device structure.\n");
		return -ENOMEM;
	}

	/* Get pointer to private data */
	this = (struct nand_chip *)(&fcr_soc_mtd[1]);

	/* Initialize structures */
	memset(fcr_soc_mtd, 0, sizeof(struct mtd_info));
	memset(this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	fcr_soc_mtd->priv = this;


	/* Set address of NAND IO lines */
	this->IO_ADDR_R = (void  *)0xbe000080;
	this->IO_ADDR_W = (void  *)0xbe000080;
	/* Set address of hardware control function */
	this->cmd_ctrl = fcr_soc_hwcontrol;
	/* 15 us command delay time */
	this->chip_delay = 35;
	this->ecc.mode = NAND_ECC_SOFT;
//	this->ecc.mode = NAND_ECC_NONE;

	/* Scan to find existence of the device */
	if (nand_scan(fcr_soc_mtd, 1)) {
		kfree(fcr_soc_mtd);
		return -ENXIO;
	}


	add_mtd_device(fcr_soc_mtd, 0, 32*1024*1024, "kernel");
	add_mtd_device(fcr_soc_mtd, 32*1024*1024, 32*1024*1024, "ramdisk");
	add_mtd_device(fcr_soc_mtd, 64*1024*1024, 420 *1024*1024, "test");
	//find_good_part(fcr_soc_mtd);

	/* Return happy */

	return 0;
}


