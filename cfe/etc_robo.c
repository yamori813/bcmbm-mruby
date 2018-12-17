/*
 * RoboSwitch setup functions
 *
 * Copyright 2005, Broadcom Corporation
 * All Rights Reserved.                
 *                                     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;   
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior      
 * written permission of Broadcom Corporation.                            
 *
 * $Id: etc_robo.c,v 1.1.1.1 2006/09/11 12:40:26 anderson Exp $
 */

// new code is bcmrobo.c

#if 0
#include <typedefs.h>
#include <osl.h>
#include <sbutils.h>
#include <sbconfig.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmparams.h>
#include <proto/ethernet.h>
#include <bcmenetmib.h>
#include <etc.h>
#include <et_dbg.h>
#endif
#include <string.h>
#include "cfe.h"
#include "etc_robo.h"
#include "sb_bp.h"

#define ET_ERROR print
#define _CFE_
#define bcmstrstr strstr
#define VLAN_MAXVID 16

#define	ETHER_ADDR_LEN	6	/* length of an Ethernet address */

#define DEVID5325	1

/*
* Switch can be programmed through SPI, MII, or OCP/SB. Each
* interface has a rreg and a wreg functions to read from and 
* write to registers.
*/
/* Device access/config oprands */
typedef struct {
	/* low level routines */
	void (*enable_mgmtif)(robo_info_t *robo);	/* enable mgmt i/f, optional */
	void (*disable_mgmtif)(robo_info_t *robo);	/* disable mgmt i/f, optional */
	int (*write_reg)(robo_info_t *robo, uint8_t page, uint8_t reg, void *val, int len);
	int (*read_reg)(robo_info_t *robo, uint8_t page, uint8_t reg, void *val, int len);
	/* description */
	char *desc;
} dev_ops_t;

/* Private state per RoboSwitch */
struct robo_info_s {
//	sb_t *sbh;			/* SiliconBackplane handle */
	bcm4401_softc_t *eth;
	void *vars;			/* nvram variables handle */
	dev_ops_t *ops;			/* device ops */
	uint32_t devid;			/* device ID (phyid) */
	uint8_t page;			/* current page */

	/* SPI */
	uint32_t ss, sck, mosi, miso;	/* GPIO mapping */

	/* MII */
	void *ch;			/* etc */
	struct chops *chop;		/* etc chop */
};

/* Constants */
#define PSEUDO_PHYAD	0x1E	/* MII Pseudo PHY address */

/* Page numbers */
#define PAGE_CTRL       0x00    /* Control page */
#define PAGE_STATUS     0x01    /* Status page */
#define PAGE_MMR        0x02    /* 5397 Management/Mirroring page */
#define PAGE_VTBL       0x05    /* ARL/VLAN Table access page */
#define PAGE_VLAN       0x34    /* VLAN page */

/* Control page registers */
#define REG_CTRL_MODE	0x0B	/* Switch Mode register */
#define REG_CTRL_MIIPO	0x0E	/* MII Port Override register */

/* VLAN page registers */
#define REG_VLAN_CTRL0	0x00	/* VLAN Control 0 register */
#define REG_VLAN_CTRL1	0x01	/* VLAN Control 1 register */
#define REG_VLAN_CTRL4	0x04	/* VLAN Control 4 register */
#define REG_VLAN_CTRL5	0x05	/* VLAN Control 5 register */
#define REG_VLAN_ACCESS	0x06	/* VLAN Table Access register */
#define REG_VLAN_WRITE	0x08	/* VLAN Write register */
#define REG_VLAN_READ	0x0C	/* VLAN Read register */
#define REG_VLAN_PTAG0	0x10	/* VLAN Default Port Tag register - port 0 */
#define REG_VLAN_PTAG1	0x12	/* VLAN Default Port Tag register - port 1 */
#define REG_VLAN_PTAG2	0x14	/* VLAN Default Port Tag register - port 2 */
#define REG_VLAN_PTAG3	0x16	/* VLAN Default Port Tag register - port 3 */
#define REG_VLAN_PTAG4	0x18	/* VLAN Default Port Tag register - port 4 */
#define REG_VLAN_PTAG5	0x1A	/* VLAN Default Port Tag register - MII port */
#define REG_VLAN_PMAP	0x20	/* VLAN Priority Re-map register */

#define VLAN_NUMVLANS	16	/* # of VLANs */

/* ARL/VLAN Table Access page registers */
#define REG_VTBL_CTRL           0x00    /* ARL Read/Write Control */
#define REG_VTBL_MINDX          0x02    /* MAC Address Index */
#define REG_VTBL_VINDX          0x08    /* VID Table Index */
#define REG_VTBL_ARL_E0         0x10    /* ARL Entry 0 */
#define REG_VTBL_ARL_E1         0x18    /* ARL Entry 1 */
#define REG_VTBL_DAT_E0         0x18    /* ARL Table Data Entry 0 */
#define REG_VTBL_SCTRL          0x20    /* ARL Search Control */
#define REG_VTBL_SADDR          0x22    /* ARL Search Address */
#define REG_VTBL_SRES           0x24    /* ARL Search Result */
#define REG_VTBL_SREXT          0x2c    /* ARL Search Result */
#define REG_VTBL_VID_E0         0x30    /* VID Entry 0 */
#define REG_VTBL_VID_E1         0x32    /* VID Entry 1 */
#define REG_VTBL_PREG           0xFF    /* Page Register */
#define REG_VTBL_ACCESS         0x60    /* VLAN table access register */
#define REG_VTBL_INDX           0x61    /* VLAN table address index register */
#define REG_VTBL_ENTRY          0x63    /* VLAN table entry register */
#define REG_VTBL_ACCESS_5395    0x80    /* VLAN table access register */
#define REG_VTBL_INDX_5395      0x81    /* VLAN table address index register */
#define REG_VTBL_ENTRY_5395     0x83    /* VLAN table entry register */

/* MII registers */
#define REG_MII_PAGE	0x10	/* MII Page register */
#define REG_MII_ADDR	0x11	/* MII Address register */
#define REG_MII_DATA0	0x18	/* MII Data register 0 */
#define REG_MII_DATA1	0x19	/* MII Data register 1 */

/* SPI registers */
#define REG_SPI_PAGE	0xff	/* SPI Page register */

/* Misc. */
#define INVAL_DEVID	-1	/* invalid PHY id */

#if 0
/*
* Access switch registers through GPIO/SPI
*/
/* Minimum timing constants */
#define SCK_EDGE_TIME	2	/* clock edge duration - 2us */
#define MOSI_SETUP_TIME	1	/* input setup duration - 1us */
#define SS_SETUP_TIME	1	/* select setup duration - 1us */

/* misc. constants */
#define SPI_MAX_RETRY	100

/* Enable GPIO access to the chip */
static void
gpio_enable(robo_info_t *robo)
{
	/* Enable GPIO outputs with SCK and MOSI low, SS high */
	sb_gpioout(robo->sbh, robo->ss | robo->sck | robo->mosi, robo->ss,GPIO_DRV_PRIORITY);
	sb_gpioouten(robo->sbh, robo->ss | robo->sck | robo->mosi, 
		robo->ss | robo->sck | robo->mosi,GPIO_DRV_PRIORITY);
}

/* Disable GPIO access to the chip */
static void
gpio_disable(robo_info_t *robo)
{
	/* Disable GPIO outputs with all their current values */
	sb_gpioouten(robo->sbh, robo->ss | robo->sck | robo->mosi, 0,GPIO_DRV_PRIORITY);
}
	
/* Write a byte stream to the chip thru SPI */
static int
spi_write(robo_info_t *robo, uint8_t *buf, uint len)
{
	uint i;
	uint8_t mask;

	/* Byte bang from LSB to MSB */
	for (i = 0; i < len; i++) {
		/* Bit bang from MSB to LSB */
		for (mask = 0x80; mask; mask >>= 1) {
			/* Clock low */
			sb_gpioout(robo->sbh, robo->sck, 0,GPIO_DRV_PRIORITY);
			OSL_DELAY(SCK_EDGE_TIME);

			/* Sample on rising edge */
			if (mask & buf[i])
				sb_gpioout(robo->sbh, robo->mosi, robo->mosi,GPIO_DRV_PRIORITY);
			else
				sb_gpioout(robo->sbh, robo->mosi, 0,GPIO_DRV_PRIORITY);
			OSL_DELAY(MOSI_SETUP_TIME);
		
			/* Clock high */
			sb_gpioout(robo->sbh, robo->sck, robo->sck,GPIO_DRV_PRIORITY);
			OSL_DELAY(SCK_EDGE_TIME);
		}
	}

	return 0;
}

/* Read a byte stream from the chip thru SPI */
static int
spi_read(robo_info_t *robo, uint8_t *buf, uint len)
{
	uint i, timeout;
	uint8_t rack, mask, byte;

	/* Timeout after 100 tries without RACK */
	for (i = 0, rack = 0, timeout = SPI_MAX_RETRY; i < len && timeout;) {
		/* Bit bang from MSB to LSB */
		for (mask = 0x80, byte = 0; mask; mask >>= 1) {
			/* Clock low */
			sb_gpioout(robo->sbh, robo->sck, 0,GPIO_DRV_PRIORITY);
			OSL_DELAY(SCK_EDGE_TIME);

			/* Sample on falling edge */
			if (sb_gpioin(robo->sbh) & robo->miso)
				byte |= mask;

			/* Clock high */
			sb_gpioout(robo->sbh, robo->sck, robo->sck,GPIO_DRV_PRIORITY);
			OSL_DELAY(SCK_EDGE_TIME);
		}
		/* RACK when bit 0 is high */
		if (!rack) {
			rack = (byte & 1);
			timeout--;
			continue;
		} 
		/* Byte bang from LSB to MSB */
		buf[i] = byte;
		i++;
	}

	if (timeout == 0) {
		ET_ERROR(("spi_read: timeout"));
		return -1;
	}

	return 0;
}

/* Enable/disable SPI access */
static void
spi_select(robo_info_t *robo, uint8_t spi)
{
	/* Enable SPI access */
	if (spi)
		sb_gpioout(robo->sbh, robo->ss, 0,GPIO_DRV_PRIORITY);
	/* Disable SPI access */
	else
		sb_gpioout(robo->sbh, robo->ss, robo->ss,GPIO_DRV_PRIORITY);
	OSL_DELAY(SS_SETUP_TIME);
}

/* Select chip and page */
static void
spi_goto(robo_info_t *robo, uint8_t page)
{
	uint8_t reg8 = REG_SPI_PAGE;	/* page select register */
	uint8_t cmd8;

	/* Issue the command only when we are on a different page */
	if (robo->page == page)
		return;
	robo->page = page;

	/* Enable SPI access */
	spi_select(robo, 1);

	/* Select new page with CID 0 */
	cmd8 = (6 << 4)		/* normal SPI */
		| 1		/* write */
		;
	spi_write(robo, &cmd8, 1);
	spi_write(robo, &reg8, 1);
	spi_write(robo, &page, 1);

	/* Disable SPI access */
	spi_select(robo, 0);
}

/* Write register thru SPI */
static int
spi_wreg(robo_info_t *robo, uint8_t page, uint8_t addr, void *val, int len)
{
	int status = 0;
	uint8_t cmd8;
	union {
		uint8_t val8;
		uint16_t val16;
		uint32_t val32;
	} bytes;

	/* validate value length and buffer address */
	ASSERT (len == 1 || (len == 2 && !((int)val & 1)) || 
		(len == 4 && !((int)val & 3)));
	
	/* Select chip and page */
	spi_goto(robo, page);

	/* Enable SPI access */
	spi_select(robo, 1);

	/* Write with CID 0 */
	cmd8 = (6 << 4)		/* normal SPI */
		| 1		/* write */
		;
	spi_write(robo, &cmd8, 1);
	spi_write(robo, &addr, 1);
	switch (len) {
	case 1:
		bytes.val8 = *(uint8_t *)val;
		break;
	case 2:
		bytes.val16 = htol16(*(uint16_t *)val);
		break;
	case 4:
		bytes.val32 = htol32(*(uint32_t *)val);
		break;
	}
	spi_write(robo, (uint8_t *)&bytes, len);

	/* Disable SPI access */
	spi_select(robo, 0);
	return status;
}

static int
spi_rreg(robo_info_t *robo, uint8_t page, uint8_t addr, void *val, int len)
{
	int status = 0;
	uint8_t cmd8;
	union {
		uint8_t val8;
		uint16_t val16;
		uint32_t val32;
	} bytes;

	/* validate value length and buffer address */
	ASSERT (len == 1 || (len == 2 && !((int)val & 1)) || 
		(len == 4 && !((int)val & 3)));
	
	/* Select chip and page */
	spi_goto(robo, page);

	/* Enable SPI access */
	spi_select(robo, 1);

	/* Fast SPI read with CID 0 and byte offset 0 */
	cmd8 = (1 << 4)		/* fast SPI */
		;
	spi_write(robo, &cmd8, 1);
	spi_write(robo, &addr, 1);
	status = spi_read(robo, (uint8_t *)&bytes, len);
	switch (len) {
	case 1:
		*(uint8_t *)val = bytes.val8;
		break;
	case 2:
		*(uint16_t *)val = ltoh16(bytes.val16);
		break;
	case 4:
		*(uint32_t *)val = ltoh32(bytes.val32);
		break;
	}

	/* Disable SPI access */
	spi_select(robo, 0);
	return status;
}
#endif

/*
* Access switch registers through MII (MDC/MDIO)
*/
/* misc. constants */
#define MII_MAX_RETRY	100

//int       robosw_r(bcm4401_softc *sc, int page, int addr);
//void      robosw_w(bcm4401_softc *sc, int page, int addr, int val);

static int
mii_rreg(robo_info_t *robo, uint8_t page, uint8_t addr, void *val, int len)
{
	*(uint16_t *)val = robosw_r(robo->eth, page, addr, len);
	return 0;
}

static int
mii_wreg(robo_info_t *robo, uint8_t page, uint8_t reg, void *val, int len)
{
	robosw_w(robo->eth, page, reg, val, len);
	return 0;
}

#if 0
/* Read register thru SPI in fast SPI mode */

/* Write register thru MDC/MDIO */
static int
mii_wreg(robo_info_t *robo, uint8_t page, uint8_t reg, void *val, int len)
{
	uint16_t cmd16, val16;
	int i;

	/* validate value length and buffer address */
	ASSERT (len == 1 || (len == 2 && !((int)val & 1)) || 
		(len == 4 && !((int)val & 3)));	

	/* set page number - MII register 0x10 */
	if (robo->page != page) {
		cmd16 = (page << 8)	/* page number */
			| 1		/* mdc/mdio access enable */
			;
		robo->chop->phywr(robo->ch, PSEUDO_PHYAD, REG_MII_PAGE, cmd16);
		robo->page = page;
	}
	/* write data - MII register 0x18-0x1B */
	switch (len) {
	case 1:
	case 2:
		val16 = (len == 1) ? *(uint8_t *)val : *(uint16_t *)val;
		robo->chop->phywr(robo->ch, PSEUDO_PHYAD, REG_MII_DATA0, val16);
		break;
	case 4:
		val16 = (uint16_t)*(uint32_t *)val;
		robo->chop->phywr(robo->ch, PSEUDO_PHYAD, REG_MII_DATA0, val16);
		val16 = (uint16_t)(*(uint32_t *)val >> 16);
		robo->chop->phywr(robo->ch, PSEUDO_PHYAD, REG_MII_DATA1, val16);
		break;
	}
	/* set register address - MII register 0x11 */
	cmd16 = (reg << 8)	/* register address */
		| 1		/* opcode write */
		;
	robo->chop->phywr(robo->ch, PSEUDO_PHYAD, REG_MII_ADDR, cmd16);
	/* is operation finished? */
	for (i = MII_MAX_RETRY; i > 0; i --) {
		val16 = robo->chop->phyrd(robo->ch, PSEUDO_PHYAD, REG_MII_ADDR);
		if ((val16 & 3) == 0)
			break;
	}
	/* timed out */
	if (!i) {
		ET_ERROR(("mii_wreg: timeout"));
		return -1;
	}
	return 0;
}

/* Read register thru MDC/MDIO */
static int
mii_rreg(robo_info_t *robo, uint8_t page, uint8_t reg, void *val, int len)
{
	uint16_t cmd16, val16;
	int i;

	/* validate value length and buffer address */
	ASSERT (len == 1 || (len == 2 && !((int)val & 1)) || 
		(len == 4 && !((int)val & 3)));
	
	/* set page number - MII register 0x10 */
	if (robo->page != page) {
		cmd16 = (page << 8)	/* page number */
			| 1		/* mdc/mdio access enable */
			;
		robo->chop->phywr(robo->ch, PSEUDO_PHYAD, REG_MII_PAGE, cmd16);
		robo->page = page;
	}
	/* set register address - MII register 0x11 */
	cmd16 = (reg << 8)	/* register address */
		| 2		/* opcode read */
		;
	robo->chop->phywr(robo->ch, PSEUDO_PHYAD, REG_MII_ADDR, cmd16);
	/* is operation finished? */
	for (i = MII_MAX_RETRY; i > 0; i --) {
		val16 = robo->chop->phyrd(robo->ch, PSEUDO_PHYAD, REG_MII_ADDR);
		if ((val16 & 3) == 0)
			break;
	}
	/* timed out */
	if (!i) {
		ET_ERROR(("mii_rreg: timeout"));
		return -1;
	}
	/* read data - MII register 0x18-0x1B */
	switch (len) {
	case 1:
	case 2:
		val16 = robo->chop->phyrd(robo->ch, PSEUDO_PHYAD, REG_MII_DATA0);
		if (len == 1)
			*(uint8_t *)val = (uint8_t)val16;
		else
			*(uint16_t *)val = val16;
		break;
	case 4:
		val16 = robo->chop->phyrd(robo->ch, PSEUDO_PHYAD, REG_MII_DATA0);
		*(uint32_t *)val = val16;
		val16 = robo->chop->phyrd(robo->ch, PSEUDO_PHYAD, REG_MII_DATA1);
		*(uint32_t *)val += val16 << 16;
		break;
	}
	return 0;
}

/*
* High level switch configuration functions.
*/

/* device oprands */
static dev_ops_t bcm5325 = {
	gpio_enable,
	gpio_disable,
	spi_wreg,
	spi_rreg,
	"bcm5325, SPI (GPIO)"
};
#endif
static dev_ops_t bcm5325e = {
	NULL,
	NULL,
	mii_wreg,
	mii_rreg,
	"bcm5325e or compatible, MII (MDC/MDIO)"
};

robo_info_t *
robo_attach(bcm4401_softc_t *eth)
{
	robo_info_t *robo;
	robo = KMALLOC(sizeof(robo_info_t), 0);
	bzero((char *)robo, sizeof(robo_info_t));
	robo->ops = &bcm5325e;
	robo->eth = eth;
	robo->devid = DEVID5325;
	return robo;
}

#if 0
/* Get access to the RoboSwitch */
robo_info_t *
robo_attach(sb_t *sbh, struct chops *chop, void *ch, void *vars)
{
	robo_info_t *robo;
	int gpio;

	/* Allocate and init private state */
	if (!(robo = MALLOC(sb_osh(sbh), sizeof(robo_info_t)))) {
		ET_ERROR(("robo_attach: out of memory, malloced %d bytes", MALLOCED(sb_osh(sbh))));
		return NULL;
	}
	bzero((char *)robo, sizeof(robo_info_t));
	robo->sbh = sbh;
	robo->vars = vars;
	robo->page = -1;
	
	/* 
	* Probe thru pseudo phyad 0x1E if the BCM5325E compatible device exists.
	* BCM5325 is not expected to respond to the query therefore phyid is -1.
	* BCM5325E and compatibles respond to MII query but the phyid may or may 
	* not be implemented therefore any non -1 phyid is valid.
	*/
	robo->devid = chop->phyrd(ch, PSEUDO_PHYAD, 2);
	robo->devid |= chop->phyrd(ch, PSEUDO_PHYAD, 3) << 16;

	/* BCM5325 */
	if (robo->devid == INVAL_DEVID) {
		/* Init GPIO mapping. Default 2, 3, 4, 5 */
		gpio = getgpiopin(vars, "robo_ss", 2);
		if (gpio == GPIO_PIN_NOTDEFINED) {
			ET_ERROR(("robo_attach: robo_ss gpio fail: GPIO 2 in use"));
			goto error;
		}
		robo->ss = 1 << gpio;
		gpio = getgpiopin(vars, "robo_sck", 3);
		if (gpio == GPIO_PIN_NOTDEFINED) {
			ET_ERROR(("robo_attach: robo_sck gpio fail: GPIO 3 in use"));
			goto error;
		}
		robo->sck = 1 << gpio; 
		gpio = getgpiopin(vars, "robo_mosi", 4);
		if (gpio == GPIO_PIN_NOTDEFINED) {
			ET_ERROR(("robo_attach: robo_mosi gpio fail: GPIO 4 in use"));
			goto error;
		}
		robo->mosi = 1 << gpio; 
		gpio = getgpiopin(vars, "robo_miso", 5);
		if (gpio == GPIO_PIN_NOTDEFINED) {
			ET_ERROR(("robo_attach: robo_miso gpio fail: GPIO 5 in use"));
			goto error;
		}
		robo->miso = 1 << gpio; 
		robo->ops = &bcm5325;
	}
	/* BCM5325E or compatible */
	else {
		/* cache etc chop */
		robo->ch = ch;
		robo->chop = chop;
		robo->ops = &bcm5325e;
	}

	/* sanity check */
	ASSERT(robo->ops);
	ASSERT(robo->ops->write_reg);
	ASSERT(robo->ops->read_reg);

	return robo;
error:
	robo_detach(robo);
	return NULL;
}

/* Release access to the RoboSwitch */
void
robo_detach(robo_info_t *robo)
{
	MFREE(sb_osh(robo->sbh), robo, sizeof(robo_info_t));
}
#endif

/* Enable the device and set it to a known good state */
int
robo_enable_device(robo_info_t *robo)
{
	int status = 0;
	uint8_t mii8;
	uint32_t reset;

	phys_write32(BCM5836_REG_ROBOSW + R_SBTMSTATELOW,
	    M_SBTS_RS | M_SBTS_CE | M_SBTS_FC);
	phys_read32(BCM5836_REG_ROBOSW + R_SBTMSTATELOW);
	cfe_usleep(100);
	phys_write32(BCM5836_REG_ROBOSW + R_SBTMSTATELOW,
	    M_SBTS_CE | M_SBTS_FC);
	phys_read32(BCM5836_REG_ROBOSW + R_SBTMSTATELOW);
	cfe_usleep(100);
	phys_write32(BCM5836_REG_ROBOSW + R_SBTMSTATELOW,
	    M_SBTS_CE);
	phys_read32(BCM5836_REG_ROBOSW + R_SBTMSTATELOW);
	cfe_usleep(100);
#if 0
	/*
	* Explicitly enable the external switch by toggling the GPIO
	* pin if nvram variable 'gpioX=robo_reset' exists. This var
	* tells that GPIO pin X is connected to the switch's RESET pin
	* and keeps the switch in reset after a POR.
	*
	*   Note: Return value 0xff from getgpiopin() means there is 
	*         no need to enable the switch or the switch is an
	*         integrated robo core.
	*/
	if ((reset = getgpiopin(robo->vars, "robo_reset", GPIO_PIN_NOTDEFINED)) == GPIO_PIN_NOTDEFINED) {
		/* Enable the core if it exists. */
		uint idx = sb_coreidx(robo->sbh);
		if (sb_setcore(robo->sbh, SB_ROBO, 0))
			sb_core_reset(robo->sbh, 0);
		sb_setcoreidx(robo->sbh, idx);
		goto rvmii_enable;
	}
	
	/*
	* External switch enable sequence: RESET low(50ms)->high(20ms)
	*
	* We have to perform a full sequence for we don't know how long
	* it has been from power on till now.
	*/
	/* convert gpio pin to gpio register mask/value */
	reset = 1 << reset;

	/* Keep RESET low for 50 ms */
	sb_gpioout(robo->sbh, reset, 0,GPIO_DRV_PRIORITY);
	sb_gpioouten(robo->sbh, reset, reset,GPIO_DRV_PRIORITY);
	bcm_mdelay(50);
	
	/* Keep RESET high for at least 20 ms */
	sb_gpioout(robo->sbh, reset, reset,GPIO_DRV_PRIORITY);
	bcm_mdelay(20);

	/*
	* Must put the switch into Reverse MII mode!
	*/
#endif
rvmii_enable:
	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);
	
	/* MII port state override (page 0 register 14) */
	robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &mii8, sizeof(mii8));

	/* Bit 4 enables reverse MII mode */
	if (!(mii8 & (1 << 4))) {
		/* Enable RvMII */
		mii8 |= (1 << 4);
		robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &mii8, sizeof(mii8));

		/* Read back */
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MIIPO, &mii8, sizeof(mii8));
		if (!(mii8 & (1 << 4))) {
			ET_ERROR(("robo_enable_device: enabling RvMII mode failed\n"));
			status = -1;
		}
	}

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);
	
	return status;
}

/* Port flags */
#define FLAG_TAGGED	't'	/* output tagged (external ports only) */
#define FLAG_UNTAG	'u'	/* input & output untagged (CPU port only, for OS (linux, ...) */
#define FLAG_LAN	'*'	/* input & output untagged (CPU port only, for bootloader (CFE, ...) */

/* Configure the VLANs */
int
robo_config_vlan(robo_info_t *robo, uint8_t *mac_addr)
{
	uint8_t val8;
	uint16_t val16;
	uint32_t val32;
	uint8_t arl_entry[8] = { 0 };
	uint8_t arl1_entry[8] = { 0 };
	/* port descriptor */
	struct {
		uint16_t untag;	/* untag enable bit (Page 0x34 Address 0x08-0x0B Bit[11:6]) */
		uint16_t member;	/* vlan member bit (Page 0x34 Address 0x08-0x0B Bit[5:0]) */
		uint8_t ptagr;	/* port tag register address (Page 0x34 Address 0x10-0x1D) */
		uint8_t cpu;	/* is this cpu port? */
	} pdesc[] = {
		/* port 0 */ {1 << 6, 1 << 0, REG_VLAN_PTAG0, 0},
		/* port 1 */ {1 << 7, 1 << 1, REG_VLAN_PTAG1, 0},
		/* port 2 */ {1 << 8, 1 << 2, REG_VLAN_PTAG2, 0},
		/* port 3 */ {1 << 9, 1 << 3, REG_VLAN_PTAG3, 0},
		/* port 4 */ {1 << 10, 1 << 4, REG_VLAN_PTAG4, 0},
		/* mii port */ {1 << 11, 1 << 5, REG_VLAN_PTAG5, 1},
	};
	uint16_t vid;

	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);
	
	/* setup global vlan configuration */
	/* VLAN Control 0 Register (Page 0x34, Address 0) */
	robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_CTRL0, &val8, sizeof(val8));
	val8 |= (1 << 7)		/* enable 802.1Q VLAN */
		| (3 << 5)	/* individual VLAN learning mode */
		;
	if (robo->devid == DEVID5325)
		val8 &= ~(1 << 1);      /* must clear reserved bit 1 */
//	robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_CTRL0, &val8, sizeof(val8));
	
	/* VLAN Control 1 Register (Page 0x34, Address 1) */
	robo->ops->read_reg(robo, PAGE_VLAN, REG_VLAN_CTRL1, &val8, sizeof(val8));
	val8 |=  (1 << 2)	/* enable RSV multicast V Fwdmap */
		| (1 << 3)	/* enable RSV multicast V Untagmap */
		;
	if (robo->devid == DEVID5325)
		val8 |= (1 << 1);       /* enable RSV multicast V Tagging */
	robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_CTRL1, &val8, sizeof(val8));
	
	arl_entry[0] = mac_addr[5];
	arl_entry[1] = mac_addr[4];
	arl_entry[2] = mac_addr[3];
	arl_entry[3] = mac_addr[2];
	arl_entry[4] = mac_addr[1];
	arl_entry[5] = mac_addr[0];

	if (robo->devid == DEVID5325) {
		robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_ARL_E1, arl1_entry, sizeof(arl1_entry));
		robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_VID_E1, arl1_entry, 1);
		arl_entry[6] = 0x8;             /* Port Id: MII */
		arl_entry[7] = 0xc0;    /* Static Entry, Valid */

		robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_ARL_E0, arl_entry,
		    sizeof(arl_entry));
		robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_MINDX, arl_entry,
		    ETHER_ADDR_LEN);
	
		/* VLAN Control 4 Register (Page 0x34, Address 4) */
		val8 = (1 << 6)		/* drop frame with VID violation */
			;
		robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_CTRL4, &val8, sizeof(val8));
		/* VLAN Control 5 Register (Page 0x34, Address 5) */
		val8 = (1 << 3)		/* drop frame when miss V table */
		;
		robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_CTRL5, &val8, sizeof(val8));
	} else {
		/* Initialize the MAC Addr Index Register */
		robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_MINDX,
		    arl_entry, ETHER_ADDR_LEN);
	}

	/* setup each vlan. max. 16 vlans. */
	/* force vlan id to be equal to vlan number */
	for (vid = 0; vid < VLAN_NUMVLANS; vid ++) {
		char vlanports[] = "vlanXXXXports";
		char port[] = "XXXX", *ports, *next, *cur;
		uint16_t untag = 0;
		uint16_t member = 0;
		int pid, len;

		/* no members if VLAN id is out of limitation */
		if (vid > VLAN_MAXVID)
			goto vlan_setup;

		/* get vlan member ports from nvram */
//		sprintf(vlanports, "vlan%dports", vid);
//		ports = getvar(robo->vars, vlanports);
		if (vid == 0)
			ports = nvram_get("vlan0ports");
		else if (vid == 1)
			ports = nvram_get("vlan1ports");
		else 
			ports = NULL;
		
		/* disable this vlan if not defined */
		if (!ports)
			goto vlan_setup;
	
		/*
		* setup each port in the vlan. cpu port needs special handing 
		* (with or without output tagging) to support linux/pmon/cfe.
		*/
		for (cur = ports; cur; cur = next) {
			/* tokenize the port list */
			while (*cur == ' ')
				cur ++;
			next = bcmstrstr(cur, " ");
			len = next ? next - cur : strlen(cur);
			if (!len)
				break;
			if (len > sizeof(port) - 1)
				len = sizeof(port) - 1;
			strncpy(port, cur, len);
			port[len] = 0;
			
			/* make sure port # is within the range */
//			pid = bcm_atoi(port);
			pid = port[0] - '0';
			if (pid >= sizeof(pdesc) / sizeof(pdesc[0])) {
				ET_ERROR(("robo_config_vlan: port %d in vlan%dports is out of range\n", pid, vid));
				continue;
			}

			/* build VLAN registers values */
#if defined(PMON) || defined(_CFE_)
			untag |= pdesc[pid].untag;
#else
			if ((!pdesc[pid].cpu && !strchr(port, FLAG_TAGGED)) ||
			    (pdesc[pid].cpu && strchr(port, FLAG_UNTAG)))
				untag |= pdesc[pid].untag;
#endif
			member |= pdesc[pid].member;

			/* set port tag - applies to untagged ingress frames */
			/* Default Port Tag Register (Page 0x34, Addres 0x10-0x1D) */
			if (!pdesc[pid].cpu ||
#if defined(PMON) || defined(_CFE_)
				strchr(port, FLAG_LAN)
#else
				strchr(port, FLAG_UNTAG)
#endif
			    ) {
				val16 = (0 << 13)	/* priority - always 0 */
					| vid		/* vlan id */
					;
				robo->ops->write_reg(robo, PAGE_VLAN, pdesc[pid].ptagr, &val16, sizeof(val16));
			}
		}

		/* Add static ARL entries */
		if (robo->devid == DEVID5325) {
			val8 = vid;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_VID_E0,
			    &val8, sizeof(val8));
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_VINDX,
			    &val8, sizeof(val8));
			/* Write the entry */
			val8 = 0x80;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_CTRL,
			    &val8, sizeof(val8));
			/* Wait for write to complete */
			while (1) {
				robo->ops->read_reg(robo, PAGE_VTBL, REG_VTBL_CTRL, &val8, sizeof(val8));
				if ((val8 & 0x80) == 0)
					break;
				cfe_usleep(100);
			}
		} else {
			/* Set the VLAN Id in VLAN ID Index Register */
			val8 = vid;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_VINDX,
			    &val8, sizeof(val8));
			/* Set the MAC addr and VLAN Id in ARL Table MAC/VID
			 * Entry 0 Register.
                         */
			arl_entry[6] = vid;
			arl_entry[7] = 0x0;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_ARL_E0,
			    arl_entry, sizeof(arl_entry));
			/* Set the Static bit , Valid bit and Port ID fields in
			 * ARL Table Data Entry 0 Register
			 */
			val16 = 0xc008;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_DAT_E0,
			    &val16, sizeof(val16));
			/* Clear the ARL_R/W bit and set the START/DONE bit in
			 * the ARL Read/Write Control Register.
			 */
			val8 = 0x80;
			robo->ops->write_reg(robo, PAGE_VTBL, REG_VTBL_CTRL,
			    &val8, sizeof(val8));
			/* Wait for write to complete */
			while (1) {
				robo->ops->read_reg(robo, PAGE_VTBL, REG_VTBL_CTRL, &val8, sizeof(val8));
				if ((val8 & 0x80) == 0)
					break;
				cfe_usleep(100);
			}
		}
			
		
		/* setup VLAN ID and VLAN memberships */
vlan_setup:
		/* VLAN Write Register (Page 0x34, Address 0x08-0x0B) */
		val32 = (1 << 20)		/* valid write */
			| ((vid >> 4) << 12)	/* vlan id bit[11:4] */
			| untag			/* untag enable */
			| member		/* vlan members */
			;
		robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_WRITE, &val32, sizeof(val32));
		/* VLAN Table Access Register (Page 0x34, Address 0x06-0x07) */
		val16 = (1 << 13) 	/* start command */
			| (1 << 12) 	/* write state */
			| vid		/* vlan id */
			;
		robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_ACCESS, &val16, sizeof(val16));
	}

	/* setup priority mapping - applies to tagged ingress frames */
	/* Priority Re-map Register (Page 0x34, Address 0x20-0x23) */
	val32 = (0 << 0)	/* 0 -> 0 */
		| (1 << 3)	/* 1 -> 1 */
		| (2 << 6)	/* 2 -> 2 */
		| (3 << 9)	/* 3 -> 3 */
		| (4 << 12)	/* 4 -> 4 */
		| (5 << 15)	/* 5 -> 5 */
		| (6 << 18)	/* 6 -> 6 */
		| (7 << 21)	/* 7 -> 7 */
		;
	robo->ops->write_reg(robo, PAGE_VLAN, REG_VLAN_PMAP, &val32, sizeof(val32));
	
	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return 0;
}

/* Enable switching/forwarding */
int
robo_enable_switch(robo_info_t *robo)
{
	int status = 0;
	uint8_t mode8;
	
	/* Enable management interface access */
	if (robo->ops->enable_mgmtif)
		robo->ops->enable_mgmtif(robo);

	/* Switch Mode register (Page 0, Address 0x0B) */
	robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MODE, &mode8, sizeof(mode8));

	/* Bit 1 enables switching/forwarding */
	if (!(mode8 & (1 << 1))) {
		/* Enable forwarding */
		mode8 |= (1 << 1);
		robo->ops->write_reg(robo, PAGE_CTRL, REG_CTRL_MODE, &mode8, sizeof(mode8));

		/* Read back */
		robo->ops->read_reg(robo, PAGE_CTRL, REG_CTRL_MODE, &mode8, sizeof(mode8));
		if (!(mode8 & (1 << 1))) {
			ET_ERROR(("robo_enable_switch: enabling forwarding failed\n"));
			status = -1;
		}
	}

	/* Disable management interface access */
	if (robo->ops->disable_mgmtif)
		robo->ops->disable_mgmtif(robo);

	return status;
}

robosw_init(bcm4401_softc_t *eth, uint8_t *mac_addr)
{
	robo_info_t *rsw;

	if (G_SBID_CR(phys_read32(BCM5836_REG_ROBOSW + R_SBIDHIGH)) ==
	    K_ROBO) {
		rsw = robo_attach(eth);
		robo_enable_device(rsw);
		robo_config_vlan(rsw, mac_addr);
		robo_enable_switch(rsw);
	}
}
