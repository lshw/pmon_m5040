/**********************************************************************
 *	Module Name : TCM Module 
 *  Module description : implement tmc's tcm module read/write function
 *  Version : V1.0
 *  Author : liwengang
 *  Email : hensinl@126.com 
 *  Date : 2009.11.6 
 **********************************************************************/
#include <stdio.h>
#include <include/cs5536.h>
#include <include/bonito.h>
#include <pci/cs5536_io.h>
#include <sys/linux/types.h>
#include <machine/pio.h>
#include <sys/linux/io.h>
#include <pmon.h>

#define I2C_TCM_CLK 14
#define I2C_TCM_DAT 15

#define	SMB_SDA				0x00
#define	SMB_STS				0x01
#define	SMB_STS_SLVSTP		(1 << 7)
#define	SMB_STS_SDAST		(1 << 6)
#define	SMB_STS_BER		(1 << 5)
#define	SMB_STS_NEGACK		(1 << 4)
#define	SMB_STS_STASTR		(1 << 3)
#define	SMB_STS_NMATCH		(1 << 2)
#define	SMB_STS_MASTER		(1 << 1)
#define	SMB_STS_XMIT		(1 << 0)
#define	SMB_CTRL_STS			0x02
#define	SMB_CSTS_TGSTL		(1 << 5)
#define	SMB_CSTS_TSDA		(1 << 4)
#define	SMB_CSTS_GCMTCH		(1 << 3)
#define	SMB_CSTS_MATCH		(1 << 2)
#define	SMB_CSTS_BB		(1 << 1)
#define	SMB_CSTS_BUSY		(1 << 0)
#define	SMB_CTRL1			0x03
#define	SMB_CTRL1_STASTRE	(1 << 7)
#define	SMB_CTRL1_NMINTE	(1 << 6)
#define	SMB_CTRL1_GCMEN		(1 << 5)
#define	SMB_CTRL1_ACK		(1 << 4)
#define	SMB_CTRL1_RSVD		(1 << 3)
#define	SMB_CTRL1_INTEN		(1 << 2)
#define	SMB_CTRL1_STOP		(1 << 1)
#define	SMB_CTRL1_START		(1 << 0)
#define	SMB_ADDR			0x04
#define	SMB_ADDR_SAEN		(1 << 7)
#define	SMB_CTRL2			0x05
#define	SMB_ENABLE		(1 << 0)
#define	SMB_CTRL3			0x06

typedef unsigned char uc8;

extern void delay(int );

int i2c_wait2(uint base){
	uc8 c;
	int i;

	delay(1000);
	for(i=0; i<20; i++)
	{
		c = linux_inb(base | SMB_STS);

		if(c & SMB_STS_BER){
			return -1;
		}
		if(c & SMB_STS_SDAST){
			return 0;
		}
		if (c & SMB_STS_NEGACK){
			return -2;
		}
	}

	return -3;
}

static void cs5536_i2c_set(uint base, int reg, int port){
	int val;

	base |= 0xbfd00000;
	val = *(volatile uint *)(base + reg);
	val |= (1 << port);
	val &= ~(1 << (port  + 16));
	*(volatile uint *)(base + reg) = val;
}
#if 0
static void cs5536_i2c_cls(uint base, int reg, int port){
	int val;

	base |= 0xbfd00000;
	val = *(volatile uint *)(base + reg);
	val |= (1 << (port + 16));
	val &= ~(1 << port);
	*(volatile uint *)(base + reg) = val;
}
#endif

int tcm_init(unsigned int base){
	uc8 tp;

	printf("Init Tcm Start\n");
	/*GPIO set as SMB*/
	cs5536_i2c_set(base, GPIOL_IN_EN, I2C_TCM_CLK);
	cs5536_i2c_set(base, GPIOL_OUT_EN, I2C_TCM_CLK);
	cs5536_i2c_set(base, GPIOL_IN_AUX1_SEL, I2C_TCM_CLK);
	cs5536_i2c_set(base, GPIOL_OUT_AUX1_SEL, I2C_TCM_CLK);
		
	cs5536_i2c_set(base, GPIOL_IN_EN, I2C_TCM_DAT);
	cs5536_i2c_set(base, GPIOL_OUT_EN, I2C_TCM_DAT);
	cs5536_i2c_set(base, GPIOL_IN_AUX1_SEL, I2C_TCM_DAT);
	cs5536_i2c_set(base, GPIOL_OUT_AUX1_SEL, I2C_TCM_DAT);

	/* SMB initial sequence*/
	
	/* disable device and config the bus clock */
	linux_outb((0x3c << 1) & 0xfe, base | SMB_CTRL2);

	/* polling mode */
	linux_outb(0x00, base | SMB_CTRL2);

	/* Disable slave address, disable slave mode */
	linux_outb(0x0, base | SMB_ADDR);

	/* Eable the bus master device */
	tp = linux_inb(base | SMB_CTRL2);
	linux_outb(tp | SMB_ENABLE, base | SMB_CTRL2);

	/* Free STALL after START */
	tp = linux_inb(base | SMB_CTRL1);
	linux_outb(tp & (~(SMB_CTRL1_STASTRE | SMB_CTRL1_NMINTE)), base | SMB_CTRL1);

	/* Send a STOP */
	tp = linux_inb(base | SMB_CTRL1);
	linux_outb(tp | SMB_CTRL1_STOP, base | SMB_CTRL1);

	/* Clear BER, NEGACK, STASTR bit  */
	linux_outb(SMB_STS_BER | SMB_STS_NEGACK | SMB_STS_STASTR, base | SMB_CTRL_STS);

	printf("Init Tcm OK\n");

	return 0;
}

#if 1
void printreg(uint base){
	uc8 tp;

	tp = linux_inb(base | 0x01);
	printf("1 : %x\t", tp);
	tp = linux_inb(base | 0x02);
	printf("2 : %x\t", tp);
	tp = linux_inb(base | 0x03);
	printf("3 : %x\t", tp);
	tp = linux_inb(base | 0x04);
	printf("4 : %x\t", tp);
	tp = linux_inb(base | 0x05);
	printf("5 : %x\t", tp);

	printf("\n");

}
#endif

int wait_ack(uint base){
	uc8 tp;

	delay(1000);
	while (1){
		tp = linux_inb(base | SMB_STS);
		if (tp & 1<<6)
			break;
	}

	return 0;
}

int send_ack(uint base){
	uc8 tp;

	tp = linux_inb(base | SMB_CTRL1);
	linux_outb(tp &0xed, base | SMB_CTRL1);	

	return 0;
}

int send_nack(uint base){
	uc8 tp;

	tp = linux_inb(base | SMB_CTRL1);
	linux_outb(tp | 1<<4, base | SMB_CTRL1);	

	return 0;
}

int init_device_status(uint base){
	int i; 
	uc8 tp1, tp2;
	
	for (i = 0; i < 10; i++){
		tp1 = linux_inb(base | 0x1);
		tp2 = linux_inb(base | 0x2);

		if (tp1 !=0x0 || tp2 != 0x10){
			linux_outb(0x30, base|SMB_STS);
			tcm_init(base);
		}
		else 
			break;
	}
	if (i == 10){
		printf("Device Error\n");

		return -1;
	}

	return 0;
}

int wait_ko(uint base){
	int i;
	uc8 c;

	for (i = 0; i < 10000; i++){
			c = linux_inb(base | 0x1);
			if ((c&0x40) == 0x40)
				break;
		}
		if (i == 10000){
			printf("Error : wait is not ko\n");
			c = linux_inb(base | 0x1);
			linux_outb(0x30,base|SMB_STS);
			tcm_init(base);
			linux_outb(2,base|SMB_CTRL1);
			
			return -1;
		}

		return 0;
}

int tcm_write(uint base, uint addr, char *buf, int len){
	int i,j; 
	uc8 tp;
	int ret = 0;
	
	/* init i2c to ko status */
	ret = init_device_status(base);  	
	if (ret == -1)
		return -1;

	/* send start condition */
	linux_outb(SMB_CTRL1_START, base | SMB_CTRL1);
	i2c_wait2(base);

	/* send slave address*/
	linux_outb(addr & 0xfe, base);
	ret = wait_ko(base);
	if (ret == -1)
		return -1;	

	/* send data */
	for (j = 0; j < len; j++){
		linux_outb(buf[j], base);

		for (i = 0; i < 100000; i++){
			tp = linux_inb(base | 0x1);
			if ((tp&0x40) == 0x00){
				delay(1000);
				break;
			}
		}
		if (i == 100000){
			return -1;
		}
	}
	

	/* send stop condition */
	linux_outb(SMB_CTRL1_STOP, base | SMB_CTRL1);
	i2c_wait2(base);

	/* clear BER, NACK*/
	tp = linux_inb(base | SMB_STS);
	linux_outb(tp | 0x30, base | SMB_STS);	

	return 0;
}

int tcm_read(unsigned int base, unsigned int addr, char *value, int len){
	int i;
	uc8 c;
	int ret = 0;

	/* init i2c to ko status */
	ret = init_device_status(base);
	if (ret == -1)
		return -1;
	
	/* send start condition */
	linux_outb(SMB_CTRL1_START, base | SMB_CTRL1);

	delay(100);
	
	/* send address */
	linux_outb(addr|1,base);
	if(i2c_wait2(base) != 0){
		printf("Error : address\n");
	}

	/* wait address send ok */
	ret = wait_ko(base);
	if (ret == -1)
		return -1;

	/* receive data */
	for (i = 0; i < len - 1; i++){
		send_ack(base);

		value[i]=linux_inb(base);
		ret = wait_ko(base);
		if (ret == -1)
			return -1;
	}
	send_nack(base);

	value[i]=linux_inb(base);
	ret = wait_ko(base);
	if (ret == -1)
		return -1;

	/* send stop condition */
	linux_outb(2,base|SMB_CTRL1);
	ret = wait_ko(base);
	if (ret == -1)
		return -1;

	c=linux_inb(base|SMB_STS);
	linux_outb(0x30,base|SMB_STS);

	return 0;
}

