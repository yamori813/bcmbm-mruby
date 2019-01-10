/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  Broadcom Silicon Backplane MAC Driver	File: dev_sb_mac.c
    *  
    *  Author:  Ed Satterthwaite
    *  
    *********************************************************************  
    *
    *  Copyright 2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#include "cfe.h"
#define blockcopy memcpy
#include "cfe_irq.h"

#include "lib_try.h"
#include "lib_physio.h"

#include "cfe_cache.h"
#include "net_enet.h"

#include "pci_machdep.h"
#include "sbmips32.h"
#include "lib_hssubr.h"
#include "addrspace.h"

//#include "pcivar.h"
//#include "pcireg.h"

#ifdef BCM47XX

/* No bcm47xx SOC tested so far has been able to communicate with a
   bcm4401 via PCI.  In early revisions of the bcm4401, accesses to
   CSRs in the DMA engine, EMAC, MIB and configuration regions (except
   SBIDHIGH) appear to complete correctly, but all the others fault
   with a bus error.  In a B0 revision bcm4401, all such accesses give
   bus errors. */
#undef  CFG_PCI
#define CFG_PCI 0

#include "sb_bp.h"
#include "sb_pci.h"
#include "sb_mac.h"

/* MII extensions shared by several Broadcom PHYs */
#define R_INTERRUPT             0x1A

#define M_PHYINT_IS             _DD_MAKEMASK1(0)        /* InterruptStatus */
#define M_PHYINT_LC             _DD_MAKEMASK1(1)        /* LinkChangeInt */
#define M_PHYINT_SP             _DD_MAKEMASK1(2)        /* SpeedChangeInt */
#define M_PHYINT_DC             _DD_MAKEMASK1(3)        /* DuplexChangeInt */
#define M_PHYINT_MI             _DD_MAKEMASK1(8)        /* MasterIntMask */
#define M_PHYINT_LI             _DD_MAKEMASK1(9)        /* LinkIntMask */
#define M_PHYINT_SI             _DD_MAKEMASK1(10)       /* SpeedIntMask */
#define M_PHYINT_FD             _DD_MAKEMASK1(11)       /* FullDuplexIntMask */
#define M_PHYINT_IE             _DD_MAKEMASK1(14)       /* InterruptEnable */

#else
#include "bcm4401.h"
#endif
#include "mii.h"

/*
   This is a driver for the Broadcom 4401 10/100 MAC with integrated
   PHY as well as for the 10/100 MAC cores used in OCP-based SOCs.

   This driver takes advantage of DMA coherence in systems that
   support it (e.g., SB1250).  For systems without coherent DMA (e.g,
   BCM47xx SOCs), packet buffer memory is explicitly flushed, and
   descriptors are referenced only in uncacheable modes.

   The BCM4401 does not have a big-endian mode for DMA.  On the
   SB1250, this driver therefore uses "preserve byte lanes" addresses
   for all DMA accesses that cross the ZBbus-PCI bridge.  Descriptors
   and packets headers as seen by the CPU must be byte-swapped for the
   DMA engine.  For hardware without such a mode, only little-endian
   operation is supported.
*/

#define K_PCI_ID_BCM4401        0x4401     /* Rev A0, A1 */
#define K_PCI_ID_BCM4401_B      0x170C     /* Rev B0 and later */
#define K_PCI_ID_BCM4402        0x4402     /* used by HND */

#ifndef B44_DEBUG
#define B44_DEBUG 0
#endif

#if ((ENDIAN_BIG + ENDIAN_LITTLE) != 1)
#error "dev_sb_mac: system endian not set"
#endif

/* Set IPOLL to drive processing through the pseudo-interrupt
   dispatcher.  Set XPOLL to drive processing by an external polling
   agent.  Setting both is ok. */

#ifndef IPOLL
#define IPOLL 0
#endif
#ifndef XPOLL
#define XPOLL 1
#endif

#define CACHE_ALIGN       32
#define PAGE_ALIGN        4096
#define ALIGN(n,align)    (((n)+((align)-1)) & ~((align)-1))

#define MIN_ETHER_PACK  (ENET_MIN_PKT+ENET_CRC_SIZE)   /* size of min packet */
#define MAX_ETHER_PACK  (ENET_MAX_PKT+ENET_CRC_SIZE)   /* size of max packet */

/* Packet buffers.  For the BCM4401, an rx packet is preceded by
   status information written into the rx buffer.  The packet itself
   begins at a programmable offset (PKTBUF_RX_OFFSET), which must be
   at least 28.  The DMA engine allows arbitrary buffer and packet
   alignment, but aligning to a cache line boundary can reduce lines
   touched on the copies.  Also, header and packet must be 32-bit
   aligned for 47xx byte swapping on RAM access to work correctly.  */

#define PKTBUF_RX_OFFSET    CACHE_ALIGN
#define ETH_PKTBUF_LEN      ALIGN(PKTBUF_RX_OFFSET+MAX_ETHER_PACK, CACHE_ALIGN)
#define ETH_PKTPOOL_SIZE    32

typedef struct eth_pkt_s {
    queue_t next;			/*  8 */
    uint8_t *buffer;			/*  4 */
    uint32_t flags;			/*  4 */
    int32_t length;			/*  4 */
    uint32_t unused[3];			/* 12 */
    uint8_t data[ETH_PKTBUF_LEN];
} eth_pkt_t;

/* Work around for apparent alignment problem with DMA (still needed?) */
/* #define ETH_PKTBUF_SIZE   ALIGN(sizeof(eth_pkt_t), CACHE_ALIGN) */
#define ETH_PKTBUF_SIZE   2048
#define ETH_PKTBUF_OFFSET (offsetof(eth_pkt_t, data))

#define ETH_PKT_BASE(data) ((eth_pkt_t *)((data) - ETH_PKTBUF_OFFSET))

static void
show_packet(char c, eth_pkt_t *pkt, int offset)
{
    int i;
    int n = (pkt->length < 32 ? pkt->length : 32);

    xprintf("%c[%4d]:", c, pkt->length);
    for (i = 0; i < n; i++) {
	if (i % 4 == 0)
	    xprintf(" ");
	xprintf("%02x", pkt->buffer[offset+i]);
	}
    xprintf("\n");
}


/* Descriptor structures.  The descriptor ring must begin on a 4K
   boundary and cannot exceed 512 entries.  Note that descriptors are
   referenced by the DMA engine using match-bytes addresses. */

typedef struct rx_dscr {
    uint32_t   rxd_cmdsts;
    pci_addr_t rxd_bufptr;
} rx_dscr;
	
typedef struct tx_dscr {
    uint32_t   txd_cmdsts;
    pci_addr_t txd_bufptr;
} tx_dscr;


/* Driver data structures */

typedef enum {
    eth_state_uninit,
    eth_state_off,
    eth_state_on, 
    eth_state_broken
} eth_state_t;

typedef struct bcm4401_softc {
    uint32_t membase;
    uint8_t irq;		/* interrupt mapping (used if IPOLL) */
    pcitag_t tag;               /* tag for configuration registers */

    /* functions for mapping between memory and dma addresses */
    uint32_t (*phys_to_dma)(uint32_t);
    uint32_t (*dma_to_phys)(uint32_t);

    uint8_t hwaddr[ENET_ADDR_LEN];                 
    uint16_t device;            /* chip device code */
    uint8_t revision;		/* chip revision and step */

    eth_state_t state;          /* current state */
    uint32_t intmask;           /* interrupt mask */

    /* These fields are set before calling bcm4401_hwinit */
    int linkspeed;		/* encodings from cfe_ioctl */
    int loopback;

    /* Packet free list */
    queue_t freelist;
    uint8_t *pktpool;
    queue_t rxqueue;

    /* The descriptor tables */
    uint8_t    *rxdscrmem;	/* receive descriptors */
    uint8_t    *txdscrmem;	/* transmit descriptors */

    /* These fields keep track of where we are in tx/rx processing */
    volatile rx_dscr *rxdscr_start;	/* beginning of ring */
    volatile rx_dscr *rxdscr_end;	/* end of ring */
    volatile rx_dscr *rxdscr_remove;	/* oldest one owned by DMA */
    volatile rx_dscr *rxdscr_add;	/* next place to put a buffer */
    int      rxdscr_onring;

    volatile tx_dscr *txdscr_start;	/* beginning of ring */
    volatile tx_dscr *txdscr_end;	/* end of ring */
    volatile tx_dscr *txdscr_remove;	/* oldest one owned by DMA */
    volatile tx_dscr *txdscr_add;	/* next place to put a buffer */

    cfe_devctx_t *devctx;

    int phy_addr;
    uint32_t phy_vendor;
    uint16_t phy_device;
    int phy_interrupt;			/* bcm5221-like MII interrupt reg */
    int slow_poll;

    /* Statistics */
    uint32_t inpkts;
    uint32_t outpkts;
    uint32_t interrupts;
    uint32_t rx_interrupts;
    uint32_t tx_interrupts;
} bcm4401_softc;


bcm4401_softc *gsc;

/* Entry to and exit from critical sections (currently relative to
   interrupts only, not SMP) */

#if CFG_INTERRUPTS
#define CS_ENTER(sc) cfe_disable_irq(sc->irq)
#define CS_EXIT(sc)  cfe_enable_irq(sc->irq)
#else
#define CS_ENTER(sc) ((void)0)
#define CS_EXIT(sc)  ((void)0)
#endif


/* Chip parameterization */

#define GP_TIMER_HZ    62500000


/* Driver parameterization */

#define MAXRXDSCR      32
#define MAXTXDSCR      32
#define MINRXRING	8


/* Prototypes */

#if CFG_PCI
static void bcm4401_ether_probe(cfe_driver_t *drv,
				unsigned long probe_a, unsigned long probe_b, 
				void *probe_ptr);
#endif
static void sb_ether_probe(cfe_driver_t *drv,
			   unsigned long probe_a, unsigned long probe_b, 
			   void *probe_ptr);


/* Address mapping macros.  Accesses in which the BCM4401 is the
   target are to registers and use match bits mode.  Accesses in which
   it is the initiator always assume little-endian responses and must
   use match bytes, per the macros below.  For big-endian hosts, the
   DMA status word must be byte-swapped. */

/* Note that PTR_TO_PHYS only works with 32-bit addresses, but then
   so does the BCM4401. */
#define PTR_TO_PHYS(x) (PHYSADDR((uintptr_t)(x)))
#define PHYS_TO_PTR(a) ((uint8_t *)KERNADDR(a))

/* The DMA engine does not have a big-endian option for descriptors
   and data.  All its accesses through the host bridge use match bytes
   mode.  The CPU must construct descriptors and headers accordingly.
   PIO accesses to the configuration and host interface registers use
   match bits.  */

#if CFG_PCI
#if ENDIAN_BIG && defined(BCM47XX)
/* XXX Not clear that hardware really supports this combination. */
static uint32_t phys_to_pci(uint32_t a) {return a + 0x50000000;}
static uint32_t pci_to_phys(uint32_t a) {return a - 0x50000000;}
#else
static uint32_t phys_to_pci(uint32_t a) {return a + 0x40000000;}
static uint32_t pci_to_phys(uint32_t a) {return a - 0x40000000;}
#endif
#endif /* CFG_PCI */

#if ENDIAN_BIG && defined(BCM47XX)
/* XXX This needs cleanup/abstraction.  The 47xx SOCs efectively have
   an endian bit that is used to swap bytes in SDRAM accesses
   (only). */
static uint32_t phys_to_sb(uint32_t a) {return a + 0x10000000;}
static uint32_t sb_to_phys(uint32_t a) {return a - 0x10000000;}
#else
static uint32_t phys_to_sb(uint32_t a) {return a;}
static uint32_t sb_to_phys(uint32_t a) {return a;}
#endif

#undef PHYS_TO_PCI
#undef PCI_TO_PHYS

#define PCI_TO_PTR(sc,a)  (PHYS_TO_PTR((*(sc)->dma_to_phys)(a)))
#define PTR_TO_PCI(sc,x)  ((*(sc)->phys_to_dma)(PTR_TO_PHYS(x)))

#if ENDIAN_BIG
#define READCSR2(sc,csr) (phys_read16((sc)->membase + ((csr)^2)))
#define WRITECSR2(sc,csr,val) (phys_write16((sc)->membase + ((csr)^2), (val)))
#else
#define READCSR2(sc,csr) (phys_read16((sc)->membase + (csr)))
#define WRITECSR2(sc,csr,val) (phys_write16((sc)->membase + (csr), (val)))
#endif
#define READCSR(sc,csr) (phys_read32((sc)->membase + (csr)))
#define WRITECSR(sc,csr,val) (phys_write32((sc)->membase + (csr), (val)))

/* Byte swap utilities: host to/from little-endian */

#if ENDIAN_BIG
#define HTOL4(x) \
    ((((x) & 0x00FF) << 24) | \
     (((x) & 0xFF00) << 8)  | \
     (((x) >> 8) & 0xFF00)  | \
     (((x) >> 24) & 0x00FF))

static uint32_t
htol4(uint32_t x)
{
    uint32_t t;

    t = ((x & 0xFF00FF00) >> 8) | ((x & 0x00FF00FF) << 8);
    return (t >> 16) | ((t & 0xFFFF) << 16);
}
#else
#define HTOL4(x) (x)
#define htol4(x) (x)
#endif

#define ltoh4 htol4   /* self-inverse */


/* Packet management */

static eth_pkt_t *
eth_alloc_pkt(bcm4401_softc *sc)
{
    eth_pkt_t *pkt;

    CS_ENTER(sc);
    pkt = (eth_pkt_t *) q_deqnext(&sc->freelist);
    CS_EXIT(sc);

    if (!pkt) return NULL;

    pkt->buffer = pkt->data;
    pkt->length = ETH_PKTBUF_LEN;
    pkt->flags = 0;

    return pkt;
}

static void
eth_free_pkt(bcm4401_softc *sc, eth_pkt_t *pkt)
{
    CS_ENTER(sc);
    q_enqueue(&sc->freelist, &pkt->next);
    CS_EXIT(sc);
}


static void
eth_initfreelist(bcm4401_softc *sc)
{
    int idx;
    uint8_t *ptr;
    eth_pkt_t *pkt;

    q_init(&sc->freelist);

    ptr = sc->pktpool;
    for (idx = 0; idx < ETH_PKTPOOL_SIZE; idx++) {
	pkt = (eth_pkt_t *) ptr;
	eth_free_pkt(sc, pkt);
	ptr += ETH_PKTBUF_SIZE;
	}
}


/* Utilities */

static const char *
bcm4401_devname(bcm4401_softc *sc)
{
//    return (sc->devctx != NULL ? cfe_device_name(sc->devctx) : "eth?");
    return ("eth0");
}


/* Descriptor ring management */

static int
bcm4401_add_rcvbuf(bcm4401_softc *sc, eth_pkt_t *pkt)
{
    volatile rx_dscr *rxd;
    volatile rx_dscr *nextrxd;

    rxd = sc->rxdscr_add;
    nextrxd = rxd+1;
    if (nextrxd == sc->rxdscr_end) {
	nextrxd = sc->rxdscr_start;
	}

    /* If the next one is the same as our remove pointer, the ring is
       considered full.  */
    if (nextrxd == sc->rxdscr_remove) return -1;

    /* Only the buffer pointer needs updating. */
    rxd->rxd_bufptr = htol4(V_DSCR1_DB(PTR_TO_PCI(sc, pkt->buffer)));

    sc->rxdscr_add = nextrxd;

    mips_wbflush();
    WRITECSR(sc, R_RCV_PTR, V_RPTR_LD(PTR_TO_PCI(sc, nextrxd) & 0xFFF));

    return 0;
}

static void
bcm4401_fillrxring(bcm4401_softc *sc)
{
    eth_pkt_t *pkt;

    CS_ENTER(sc);
    while (1) {
	if (sc->rxdscr_onring >= MINRXRING) {
	    CS_EXIT(sc);
	    break;
	    }
	CS_EXIT(sc);
	pkt = eth_alloc_pkt(sc);
	if (pkt == NULL) {
	    /* could not allocate a buffer */
	    break;
	    }
	if (bcm4401_add_rcvbuf(sc, pkt) != 0) {
	    /* could not add buffer to ring */
	    eth_free_pkt(sc, pkt);
	    break;
	    }
	CS_ENTER(sc);
	sc->rxdscr_onring++;
	}
}


/*  Receive buffer processing. */

static void
bcm4401_rx_callback(bcm4401_softc *sc, eth_pkt_t *pkt)
{
    if (B44_DEBUG) show_packet('>', pkt, PKTBUF_RX_OFFSET);
    CS_ENTER(sc);
    net_rcv(pkt->data, pkt->length);
//    q_enqueue(&sc->rxqueue, &pkt->next);
    CS_EXIT(sc);
    eth_free_pkt(sc, pkt);
    sc->inpkts++;
}

static void
bcm4401_procrxring(bcm4401_softc *sc)
{
    uint32_t rxstat;
    volatile rx_dscr *rxcurr;
    volatile rx_dscr *rxd;
    eth_pkt_t *pkt;
    eth_pkt_t *newpkt;
    uint32_t hdr0;

    rxstat = READCSR(sc, R_RCV_STATUS);
    rxcurr = (volatile rx_dscr *)
               ((uint8_t *)sc->rxdscr_start + G_RSTAT_CD(rxstat));
    
    for (;;) {
	rxd = sc->rxdscr_remove;

	if (rxd == rxcurr) {
	    /* all packets processed */
	    break;
	    }

	pkt = ETH_PKT_BASE(PCI_TO_PTR(sc, (ltoh4(rxd->rxd_bufptr))));

	CACHE_DMA_INVAL(pkt->buffer, PKTBUF_RX_OFFSET);
	hdr0 = ltoh4(*(uint32_t *)(pkt->buffer));

	/* Drop error packets. */
	/* The header word apparently reports this (undocumented for 4401,
	   but documented in the 4710 manual. */
	if (hdr0 & M_RCVHDR0_ERRORS) {
	    xprintf("BCM4401: rx error %08X\n", hdr0);
	    newpkt = pkt;           /* recycle the buffer */
	    }
	else {
	    /* Pass up the packet */
	    pkt->length = G_RCVHDR0_CD(hdr0) - ENET_CRC_SIZE;
	    bcm4401_rx_callback(sc, pkt);
	    /* put a buffer back on the ring to replace this one */
	    newpkt = eth_alloc_pkt(sc);
	    }

	/* update the pointer, accounting for buffer wrap. */
	rxd++;
	if (rxd == sc->rxdscr_end)
	    rxd = sc->rxdscr_start;
	sc->rxdscr_remove = rxd;

	if (newpkt) {
	    /* The ring must have space now. */
	    bcm4401_add_rcvbuf(sc, newpkt);
	    }
	else {
	    CS_ENTER(sc);
	    sc->rxdscr_onring--;
	    CS_EXIT(sc);
	    }
	}

    /* XXX Check for error stops. */
}


/*  Transmit ring processing. */

static int
bcm4401_add_txbuf(bcm4401_softc *sc, eth_pkt_t *pkt)
{
    volatile tx_dscr *txd;
    volatile tx_dscr *nexttxd;
    uint32_t cmdsts;

    txd = sc->txdscr_add;
    cmdsts = M_DSCR0_SF | M_DSCR0_EF | M_DSCR0_IC;
    nexttxd = (txd+1);
    if (nexttxd == sc->txdscr_end) {
	cmdsts |= M_DSCR0_ET;
	nexttxd = sc->txdscr_start;
	}

    /* If the next one is the same as our remove pointer, the ring is
       considered full.  */
    if (nexttxd == sc->txdscr_remove) return -1;

    txd->txd_bufptr = htol4(V_DSCR1_DB(PTR_TO_PCI(sc, pkt->buffer)));
    cmdsts |= V_DSCR0_BC(pkt->length);
    txd->txd_cmdsts = htol4(cmdsts);

    sc->txdscr_add = nexttxd;

    return 0;
}


static int
bcm4401_transmit(bcm4401_softc *sc, eth_pkt_t *pkt)
{
    int rv;

    if (B44_DEBUG) show_packet('<', pkt, 0);

    rv = bcm4401_add_txbuf(sc, pkt);
    if (rv == 0) {
	mips_wbflush();
	WRITECSR(sc, R_XMT_PTR,
		 V_XPTR_LD(PTR_TO_PCI(sc, sc->txdscr_add) & 0xFFF));
	}
 
    sc->outpkts++;
    return rv;
}

static void
bcm4401_proctxring(bcm4401_softc *sc)
{
    uint32_t txstat;
    volatile tx_dscr *txcurr;
    volatile tx_dscr *txd;
    eth_pkt_t *pkt;

    txstat = READCSR(sc, R_XMT_STATUS);
    txcurr = (volatile tx_dscr *)
               ((uint8_t *)sc->txdscr_start + G_XSTAT_CD(txstat));

    for (;;) {
	txd = sc->txdscr_remove;

	if (txd == txcurr) {
	    /* ring is empty, no buffers to process */
	    break;
	    }

	/* Just free the packet */
	pkt = ETH_PKT_BASE(PCI_TO_PTR(sc, ltoh4(txd->txd_bufptr)));
	eth_free_pkt(sc, pkt);

	/* update the pointer, accounting for buffer wrap. */
	txd++;
	if (txd == sc->txdscr_end)
	    txd = sc->txdscr_start;

	sc->txdscr_remove = txd;
	}

    /* XXX Check for error halt. */
}


static void
bcm4401_initrings(bcm4401_softc *sc)
{
    volatile tx_dscr *txd;
    volatile rx_dscr *rxd;

    for (txd = sc->txdscr_start; txd != sc->txdscr_end; txd++) {
        txd->txd_cmdsts = HTOL4(M_DSCR0_SF | M_DSCR0_EF | M_DSCR0_IC);
	txd->txd_bufptr = 0;
	}
    (txd-1)->txd_cmdsts |= HTOL4(M_DSCR0_ET);

    for (rxd = sc->rxdscr_start; rxd != sc->rxdscr_end; rxd++) {
	rxd->rxd_cmdsts = HTOL4(M_DSCR0_SF | M_DSCR0_EF     /* XXX needed? */
				| V_DSCR0_BC(ETH_PKTBUF_LEN));
	rxd->rxd_bufptr = 0;
	}
    (rxd-1)->rxd_cmdsts |= HTOL4(M_DSCR0_ET);

    sc->txdscr_add = sc->txdscr_remove = sc->txdscr_start;
    sc->rxdscr_add = sc->rxdscr_remove = sc->rxdscr_start;
    sc->rxdscr_onring = 0;

    /* Precharge the receive ring */
    bcm4401_fillrxring(sc);
}


/* Allocate an integral number of cache lines suitable for DMA access. */
static uint8_t *
dma_alloc(size_t size, unsigned int align)
{
    uint8_t *base;
    size_t len = ALIGN(size, CACHE_ALIGN);

    base = KMALLOC(len, ALIGN(align, CACHE_ALIGN));
    if (base != NULL)
	CACHE_DMA_INVAL(base, len);
    return base;
}

static int
bcm4401_init(bcm4401_softc *sc)
{
    /* Allocate descriptor rings */
    sc->rxdscrmem = CACHE_DMA_SHARED(dma_alloc(MAXRXDSCR*sizeof(rx_dscr), PAGE_ALIGN));
    sc->txdscrmem = CACHE_DMA_SHARED(dma_alloc(MAXTXDSCR*sizeof(tx_dscr), PAGE_ALIGN));

    /* Allocate buffer pool (4K aligned to fix apparent DMA alignment bug) */
    sc->pktpool = dma_alloc(ETH_PKTPOOL_SIZE*ETH_PKTBUF_SIZE, PAGE_ALIGN);
    if (sc->pktpool == NULL) {
	xprintf("%s: No buffer memory available.\n", bcm4401_devname(sc));
	return -1;
	}
    eth_initfreelist(sc);
    q_init(&sc->rxqueue);

    /* Fill in pointers to the rings */
    sc->rxdscr_start = (volatile rx_dscr *) (sc->rxdscrmem);
    sc->rxdscr_end = sc->rxdscr_start + MAXRXDSCR;

    sc->txdscr_start = (volatile tx_dscr *) (sc->txdscrmem);
    sc->txdscr_end = sc->txdscr_start + MAXTXDSCR;

    bcm4401_initrings(sc);

    return 0;
}


static void
bcm4401_resetrings(bcm4401_softc *sc)
{
    volatile tx_dscr *txd;
    volatile rx_dscr *rxd;
    eth_pkt_t *pkt;

    /* Free any pending transmit packets (sent and unsent) */
    txd = sc->txdscr_remove;
    while (txd != sc->txdscr_add) {
	pkt = ETH_PKT_BASE(PCI_TO_PTR(sc, ltoh4(txd->txd_bufptr)));
	eth_free_pkt(sc, pkt);

	txd++;
	if (txd == sc->txdscr_end)
	    txd = sc->txdscr_start;
        }
    sc->txdscr_remove = txd;

    /* Discard any received packets as well as all free buffers */
    rxd = sc->rxdscr_remove;
    while (rxd != sc->rxdscr_add) {
	pkt = ETH_PKT_BASE(PCI_TO_PTR(sc, ltoh4(rxd->rxd_bufptr)));
	eth_free_pkt(sc, pkt);
	
	rxd++;
	if (rxd == sc->rxdscr_end)
	    rxd = sc->rxdscr_start;
	CS_ENTER(sc);
	sc->rxdscr_onring--;
	CS_EXIT(sc);
	}
    sc->rxdscr_remove = rxd;

    /* Reestablish the initial state. */
    bcm4401_initrings(sc);
}


/* CRC */


/* EEPROM access */


/* MII access */

static void
mii_enable(bcm4401_softc *sc)
{
    uint32_t devctl, enetctl;

    devctl = READCSR(sc, R_DEV_CONTROL);
    if ((devctl & M_DVCTL_IP) != 0) {
	WRITECSR(sc, R_MII_STATUS_CONTROL, M_MIICTL_PR | V_MIICTL_MD(0xD));
	devctl = READCSR(sc, R_DEV_CONTROL);
	if ((devctl & M_DVCTL_ER) != 0) {
	    devctl &= ~M_DVCTL_ER;
	    WRITECSR(sc, R_DEV_CONTROL, devctl);
	    cfe_usleep(100);
	    }
	}
    else {
	WRITECSR(sc, R_MII_STATUS_CONTROL, M_MIICTL_PR | V_MIICTL_MD(0x9));
	enetctl = READCSR(sc, R_ENET_CONTROL);
	enetctl |= M_ECTL_EP;
	WRITECSR(sc, R_ENET_CONTROL, enetctl);
	}
}

static uint16_t
mii_read(bcm4401_softc *sc, int reg)
{
    uint32_t cmd, status;
    uint32_t data;
    int timeout;

    WRITECSR(sc, R_ENET_INT_STATUS, M_EINT_MI);
    (void)READCSR(sc, R_ENET_INT_STATUS);

    cmd = (V_MIIDATA_OP(K_MII_OP_READ) | V_MIIDATA_TA(K_TA_VALID) |
           V_MIIDATA_RA(reg) | V_MIIDATA_PM(sc->phy_addr));
    WRITECSR(sc, R_MII_DATA, cmd | V_MIIDATA_SB(K_MII_START));

    for (timeout = 1000; timeout > 0; timeout -= 100) {
	status = READCSR(sc, R_ENET_INT_STATUS);
	if ((status & M_EINT_MI) != 0)
	    break;
	cfe_usleep(100);
	}

    if (timeout <= 0)
	return 0xFFFF;

    data = G_MIIDATA_D(READCSR(sc, R_MII_DATA));
    return data;
}

static void
mii_write(bcm4401_softc *sc, int reg, uint16_t value)
{
    uint32_t cmd, status;
    int timeout;

    WRITECSR(sc, R_ENET_INT_STATUS, M_EINT_MI);
    (void)READCSR(sc, R_ENET_INT_STATUS);

    cmd = (V_MIIDATA_OP(K_MII_OP_WRITE) | V_MIIDATA_TA(0x2) |
           V_MIIDATA_RA(reg) | V_MIIDATA_PM(sc->phy_addr) |
	   V_MIIDATA_D(value));
    WRITECSR(sc, R_MII_DATA, cmd | V_MIIDATA_SB(K_MII_START));

    for (timeout = 1000; timeout > 0; timeout -= 100) {
	status = READCSR(sc, R_ENET_INT_STATUS);
	if ((status & M_EINT_MI) != 0)
	    break;
	cfe_usleep(100);
	}
}

/* For an integrated PHY (bcm4401), unimplemented PHY addresses return
   id's of 0.  For (some?) external PHYs, unimplmented addresses
   appear to return 0x1FFF or 0x3FFF for id1 but reliably (?) return
   0xFFFF for id2.  */
static int
mii_probe(bcm4401_softc *sc)
{
    int i;
    uint16_t id1, id2;
    int prev = sc->phy_addr;

    for (i = 0; i < 32; i++) {
        sc->phy_addr = i;
        id1 = mii_read(sc, R_PHYIDR1);
	id2 = mii_read(sc, R_PHYIDR2);
	if (id2 != 0x0000 && id2 != 0xFFFF) {
	    sc->phy_vendor = ((uint32_t)id1 << 6) | ((id2 >> 10) & 0x3F);
	    sc->phy_device = (id2 >> 4) & 0x3F;
	    xprintf("phy %d, vendor %06x part %02x\n",
		    i, sc->phy_vendor, sc->phy_device);
	    
#if 0
	    return 0;
#endif
	    }
	}
#if 0
    xprintf("mii_probe: No PHY found\n");
#else
    xprintf("mii_probe: Using PHY %d\n", prev);
#endif
    sc->phy_addr = prev;   /* Expected addr (if any) */
    return -1;
}

#if B44_DEBUG
static void
mii_dump(bcm4401_softc *sc, const char *label)
{
    int i;
    uint16_t  r;
    uint32_t  idr, part;

    xprintf("%s, MII:\n", label);
    idr = part = 0;

    /* Common registers */
    for (i = 0x0; i <= 0x8; ++i) {
	r = mii_read(sc, i);
	xprintf(" REG%02X: %04X", i, r);
	if (i % 4 == 3) xprintf("\n");
	if (i == MII_PHYIDR1) {
	    idr |= r << 6;
	    }
	else if (i == MII_PHYIDR2) {
	    idr |= (r >> 10) & 0x3F;
	    part = (r >> 4) & 0x3F;
	    }
	}
    xprintf("\nIDR %06x, PART %02x\n", idr, part);
    
    /* Broadcom extensions */
    for (i = 0x10; i <= 0x14; ++i) {
	r = mii_read(sc, i);
	xprintf(" REG%02X: %04X", i, r);
	if (i % 4 == 3) xprintf("\n");
	}
    xprintf("\n");

    /* Broadcom extensions (52xx family) */
    for (i = 0x18; i <= 0x1F; i++) {
	r = mii_read(sc, i);
	xprintf(" REG%02X: %04X", i, r);
	if (i % 4 == 3) xprintf("\n");
	}
    xprintf("\n");
}
#else
#define mii_dump(sc,label)
#endif

static void
mii_set_speed(bcm4401_softc *sc, int speed)
{
    /* NYI */
}

static uint16_t
mii_interrupt(bcm4401_softc *sc)
{
    /* The read also clears any interrupt bits. */
    return mii_read(sc, R_INTERRUPT);
}

#define ROBOSW_ACCESS_CONTROL_REG	0x10
#define ROBOSW_RW_CONTROL_REG		0x11
#define ROBOSW_DATA_REG_BASE		0x18

#define ACCESS_CONTROL_RW		0x0001
#define RW_CONTROL_WRITE		0x0001
#define RW_CONTROL_READ			0x0002

int
robosw_r(bcm4401_softc *sc, int page, int addr, int len)
{
    mii_write(sc, ROBOSW_ACCESS_CONTROL_REG, (page << 8) | ACCESS_CONTROL_RW);
    mii_write(sc, ROBOSW_RW_CONTROL_REG, (addr << 8) | RW_CONTROL_READ);
    while(mii_read(sc, ROBOSW_RW_CONTROL_REG) & 0x0003)
        ;
    int res = mii_read(sc, ROBOSW_DATA_REG_BASE);
    if (len == 4)
      res |= mii_read(sc, ROBOSW_DATA_REG_BASE+1) << 16;
//xprintf("MORI MORI read %d %d %d %d\n", page, addr, res, len);
    return res;
}

void
robosw_w(bcm4401_softc *sc, int page, int addr, void *val, int len)
{
uint16_t val16;
uint8_t *ptr = (uint8_t *)val;

    mii_write(sc, ROBOSW_ACCESS_CONTROL_REG, (page << 8) | ACCESS_CONTROL_RW);
    switch (len) {
    case 1:
    case 2:
      val16 = (len == 1) ? *(uint8_t *)val : *(uint16_t *)val;
      mii_write(sc, ROBOSW_DATA_REG_BASE, val16);
      break;
    case 4:
      val16 = (uint16_t)*(uint32_t *)val;
      mii_write(sc, ROBOSW_DATA_REG_BASE, val16);
      val16 = (uint16_t)(*(uint32_t *)val >> 16);
      mii_write(sc, ROBOSW_DATA_REG_BASE+1, val16);
      break;
    case 8:
      val16 = ptr[7];
      val16 = ((val16 << 8) | ptr[6]);
      mii_write(sc, ROBOSW_DATA_REG_BASE+3, val16);
      /* FALLTHRU */
    case 6:
      val16 = ptr[5];
      val16 = ((val16 << 8) | ptr[4]);
      mii_write(sc, ROBOSW_DATA_REG_BASE+2, val16);
      val16 = ptr[3];
      val16 = ((val16 << 8) | ptr[2]);
      mii_write(sc, ROBOSW_DATA_REG_BASE+1, val16);
      val16 = ptr[1];
      val16 = ((val16 << 8) | ptr[0]);
      mii_write(sc, ROBOSW_DATA_REG_BASE+0, val16);
      break;
    }
//xprintf("MORI MORI write %d %d %x %d\n", page, addr, val16, len);
    mii_write(sc, ROBOSW_RW_CONTROL_REG, (addr << 8) | RW_CONTROL_WRITE);
    while(mii_read(sc, ROBOSW_RW_CONTROL_REG) & 0x0003)
        ;
}

static void
mii_autonegotiate(bcm4401_softc *sc)
{
    uint16_t  control, status, remote;
    unsigned int  timeout;
    int linkspeed;

    linkspeed = ETHER_SPEED_UNKNOWN;

    xprintf("%s: Link speed: ", bcm4401_devname(sc));

    if (sc->phy_addr == 0x1E) {
	/* XXX for SOC parts, this address may indicate a Roboswitch. */
	xprintf("100BaseT FDX (switch)\n");
	linkspeed = ETHER_SPEED_100FDX;	 
	/* Todo: may be robosw code is old. need newer code merge */
	if(!isbcm5354())
	    robosw_init(sc, sc->hwaddr);
	}        
    else {
	/* Read twice to clear latching bits */
	status = mii_read(sc, MII_BMSR);
	status = mii_read(sc, MII_BMSR);

	if ((status & (BMSR_AUTONEG | BMSR_LINKSTAT)) ==
	    (BMSR_AUTONEG | BMSR_LINKSTAT))
	    control = mii_read(sc, MII_BMCR);
	else {
	    for (timeout = 4*CFE_HZ; timeout > 0; timeout -= CFE_HZ/2) {
		status = mii_read(sc, MII_BMSR);
		if ((status & BMSR_ANCOMPLETE) != 0)
		    break;
		cfe_sleep(CFE_HZ/2);
		}
	    }

	remote = mii_read(sc, MII_ANLPAR);
	if ((status & BMSR_ANCOMPLETE) != 0) {
	    /* A link partner was negotiated... */

	    if ((remote & ANLPAR_TXFD) != 0) {
		xprintf("100BaseT FDX\n");
		linkspeed = ETHER_SPEED_100FDX;	 
		}
	    else if ((remote & ANLPAR_TXHD) != 0) {
		xprintf("100BaseT HDX\n");
		linkspeed = ETHER_SPEED_100HDX;	 
		}
	    else if ((remote & ANLPAR_10FD) != 0) {
		xprintf("10BaseT FDX\n");
		linkspeed = ETHER_SPEED_10FDX;	 
		}
	    else if ((remote & ANLPAR_10HD) != 0) {
		xprintf("10BaseT HDX\n");
		linkspeed = ETHER_SPEED_10HDX;	 
		}
	    }
	else {
	    /* no link partner convergence */
	    xprintf("Unknown\n");
	    linkspeed = ETHER_SPEED_UNKNOWN;
	    }
	sc->linkspeed = linkspeed;

	/* clear latching bits */
	status = mii_read(sc, MII_BMSR);
	}

    mii_dump(sc, "final PHY");
}


static int
bcm4401_reset(bcm4401_softc *sc)
{
    return -1;
}


/* SPROM access routines.  Random read access to the SPROM will
   produce a bus error due to PCI timeouts.  As an apparent
   (undocumented) side effect, the requested word will be fetched to a
   local buffer so that the next access will succeed.  */

#define SPROM_SIZE     0x80

static int
sprom_read_all(bcm4401_softc *sc, uint8_t dest[])
{
    int i;
    uint32_t t;
    jmpbuf_t *jb;

    jb = exc_initialize_block();
    if (jb == NULL)
	return -1;

    for (i = 0; i < SPROM_SIZE; i += 4) {
	if (exc_try(jb) == 0) {
	    /* On pass 2 parts, the following read gets a bus error */
	    t = READCSR(sc, SPROM_BASE + i);
	    cfe_usleep(1000);
	    /* the read with valid data (pass 1). */
	    t = READCSR(sc, SPROM_BASE + i);
	    }
	else {
	    /* and this one doesn't (pass 2). */
	    cfe_usleep(1000);   /* Delay needed; value is empirical. */
	    t = READCSR(sc, SPROM_BASE + i);
	    }

	dest[i+0] = (t >> 8) & 0xFF;  dest[i+1] = (t >> 0) & 0xFF;
	t >>= 16;
	dest[i+2] = (t >> 8) & 0xFF;  dest[i+3] = (t >> 0) & 0xFF;

	/* The following is a kludge, but otherwise setjmp is a one-shot. */
	exc_handler.catch_exc = 1;
	}

    exc_cleanup_block(jb);
    return 0;
}

static void
sprom_dump(uint8_t srom[])
{
    int  i;

    xprintf("BCM4401: SPROM data:");
    for (i = 0; i < SPROM_SIZE; i++) {
	if (i % 16 == 0)
	    xprintf("\n %02x: ", i);
	xprintf(" %02x", srom[i]);
	}
    xprintf("\n");
}


static int
bcm4401_set_hw_addr(bcm4401_softc *sc, uint8_t addr[])
{
    uint32_t enet_upper, enet_lower;
    int timeout;

    enet_upper = (addr[0] << 8) | addr[1];
    enet_lower = (addr[2] << 24) | (addr[3] << 16) | (addr[4] << 8) | addr[5];
    
    WRITECSR(sc, R_CAM_DATA_H, M_CAM_VB | V_CAM_CD_H(enet_upper));
    WRITECSR(sc, R_CAM_DATA_L, V_CAM_CD_L(enet_lower));

    WRITECSR(sc, R_CAM_CONTROL, V_CAMCTL_IX(0) | M_CAMCTL_CW);
    for (timeout = CFE_HZ; timeout > 0; timeout -= CFE_HZ/10) {
	if ((READCSR(sc, R_CAM_CONTROL) & M_CAMCTL_CB) != 0)
	    break;
	cfe_sleep(1);
	}
    if (timeout <= 0)
        return -1;

    return 0;
}


static void
bcm4401_set_linkspeed(bcm4401_softc *sc)
{
    uint32_t ctrl;

    ctrl = READCSR(sc, R_XMT_CONTROL1);
    switch (sc->linkspeed) {
	case ETHER_SPEED_100FDX:
	case ETHER_SPEED_10FDX:
	    ctrl |= (M_TCTL_FD | M_TCTL_SB);
	    break;
	default:
	    ctrl &= ~(M_TCTL_FD | M_TCTL_SB);
	    break;
	}
    WRITECSR(sc, R_XMT_CONTROL1, ctrl);
}

static void
bcm4401_hwinit(bcm4401_softc *sc)
{
    if (sc->state == eth_state_uninit) {
	uint32_t ctrl;

	bcm4401_reset(sc);

	mii_probe(sc);
	mii_dump(sc, "initial PHY");

	sc->phy_interrupt = 0;
	if (sc->phy_vendor == OUI_BCMxx) {
	    uint16_t phy_isr;

	    switch (sc->phy_device) {
		case DEV_BCM5201:  case DEV_BCM5221:
		case DEV_BCM4401:
		    phy_isr = mii_read(sc, R_INTERRUPT);
		    phy_isr &= ~(M_PHYINT_LI | M_PHYINT_SI | M_PHYINT_FD |
				M_PHYINT_MI);
		    phy_isr |= M_PHYINT_IE;
		    mii_write(sc, R_INTERRUPT, phy_isr);
		    sc->phy_interrupt = 1;
		    break;
		default:
		    break;
		}
	    }
	mii_autonegotiate(sc);
	if (sc->phy_interrupt) {
	    (void)mii_read(sc, R_INTERRUPT);  /* clear any pending */  
	    }
	
	if (!isbcm5350())
		bcm4401_set_hw_addr(sc, sc->hwaddr);
	WRITECSR(sc, R_CAM_CONTROL, M_CAMCTL_CE);

	/* XXX Set the transmit watermark here, if needed. */

	/* Initialize the receive channel. */
	WRITECSR(sc, R_RCV_PTR, 0);
	WRITECSR(sc, R_RCV_CONTROL, M_RCTL_RE | V_RCTL_RO(PKTBUF_RX_OFFSET));
	WRITECSR(sc, R_RCV_ADDR, PTR_TO_PCI(sc, sc->rxdscrmem));
	WRITECSR(sc, R_RCV_PTR, PTR_TO_PCI(sc, sc->rxdscr_add) & 0xFFF);

	/* Initialize the transmit channel. */
	WRITECSR(sc, R_XMT_PTR, 0);
	WRITECSR(sc, R_XMT_CONTROL, M_XCTL_XE);
	WRITECSR(sc, R_XMT_ADDR, PTR_TO_PCI(sc, sc->txdscrmem));

	/* Modify Ethernet RX MAC settings (probably obsolete). */
	WRITECSR(sc, R_EMAC_XMT_MAX_BURST, 32);
	WRITECSR(sc, R_EMAC_RCV_MAX_BURST, 32);
#if 0
	WRITECSR(sc, R_RCV_CONFIG, M_RCFG_AM);   /* All multicast */
#else
	WRITECSR(sc, R_RCV_CONFIG, 0);
#endif
	WRITECSR(sc, R_RCV_MAX_LENGTH, MAX_ETHER_PACK);

	ctrl = READCSR(sc, R_EMAC_CONTROL);
	ctrl |= M_EMCTL_CC;
	WRITECSR(sc, R_EMAC_CONTROL, ctrl);

	if (!isbcm5350())
		bcm4401_set_linkspeed(sc);

	WRITECSR(sc, R_XMT_MAX_LENGTH, MAX_ETHER_PACK);

	WRITECSR(sc, R_INT_RECV_LAZY, V_INTLZY_FC(1) | V_INTLZY_TO(100));

	/* Enable the MAC */
	if (!isbcm5350())
		ctrl = READCSR(sc, R_ENET_CONTROL);
	ctrl |= M_ECTL_EE;
	WRITECSR(sc, R_ENET_CONTROL, ctrl);

	sc->state = eth_state_on;
	}
}


static void
bcm4401_setspeed(bcm4401_softc *sc, int speed)
{
    /* XXX Not yet implemented - autonegotiation only. */
    (void)mii_set_speed;
}

static void
bcm4401_setloopback(bcm4401_softc *sc, int mode)
{
    /* XXX Not yet implemented. */
}


static void
bcm4401_isr(void *arg)
{
    bcm4401_softc *sc = (bcm4401_softc *)arg;
    uint32_t status;

#if IPOLL
    sc->interrupts++;
#endif

    for (;;) {

	/* Read and clear the interrupt status. */
	status = READCSR(sc, R_INT_STATUS);
	status &= sc->intmask;
	if (status == 0)
	    break;

	WRITECSR(sc, R_INT_STATUS, status);  /* write-to-clear */

	/* XXX Handle SERR, etc. */

	if (status & M_INT_RI) {
#if IPOLL
	    sc->rx_interrupts++;
#endif
	    bcm4401_procrxring(sc);
	    }

	if (status & M_INT_XI) {
#if IPOLL
	    sc->tx_interrupts++;
#endif
	    bcm4401_proctxring(sc);
	    }

	if (status & M_INT_TO) {
	    sc->slow_poll = 1;
	    }

	if (status & (M_INT_XU | M_INT_RO)) {
	    if (status & M_INT_XU) {
		xprintf("BCM4401: tx underrun, %08x\n", status);
		/* XXX Try to restart */
		}
	    if (status & M_INT_RO) {
		xprintf("BCM4401: rx overrun, %08x\n", status);
		/* XXX Try to restart */
		}
	    }
	}
}


static void
bcm4401_start(bcm4401_softc *sc)
{
    uint32_t status;

    bcm4401_hwinit(sc);

    /* Set up loopback here */

    WRITECSR(sc, R_GP_TIMER, 0);             /* stop the timer */

    sc->intmask = 0;
    WRITECSR(sc, R_INT_MASK, 0);
    status = READCSR(sc, R_INT_STATUS);         /* clear any pending */
    WRITECSR(sc, R_INT_STATUS, status);

    sc->intmask =  (M_INT_RI | M_INT_XI);    /* XXX add errors */
    if (sc->phy_interrupt)
	sc->intmask |=  M_INT_TO;

#if IPOLL
    cfe_request_irq(sc->irq, bcm4401_isr, sc, CFE_IRQ_FLAGS_SHARED, 0);
    WRITECSR(sc, R_INT_MASK, sc->intmask);
#endif

    sc->slow_poll = 0;
    if (sc->phy_interrupt) {
	WRITECSR(sc, R_GP_TIMER, GP_TIMER_HZ/4);
	}

    sc->state = eth_state_on;
}

static void
bcm4401_stop(bcm4401_softc *sc)
{
    uint32_t ctl, status;
    int i;

    /* Cancel the timer */
    if (sc->phy_interrupt) {
	WRITECSR(sc, R_GP_TIMER, 0);
	(void)READCSR(sc, R_GP_TIMER);
	}
    sc->slow_poll = 0;

    /* Make sure that no further interrupts will be processed. */
    sc->intmask = 0;
    WRITECSR(sc, R_INT_MASK, 0);
    (void)READCSR(sc, R_INT_MASK);  /* push */
    status = READCSR(sc, R_INT_STATUS);
    WRITECSR(sc, R_INT_STATUS, status);   /* write-to-clear */

    /* Shut down MAC */
    WRITECSR(sc, R_ENET_CONTROL, M_ECTL_ED);
    for (i = 1000; i > 0; i--) {
	ctl = READCSR(sc, R_ENET_CONTROL);
	if ((ctl & M_ECTL_ED) == 0)
	    break;
	cfe_usleep(100);
	}
    if (i == 0)
	xprintf("%s: cannot clear MAC\n", bcm4401_devname(sc));

    /* Shut down DMA engines */
    WRITECSR(sc, R_XMT_CONTROL, 0);
    for (i = 1000; i > 0; i--) {
	status = READCSR(sc, R_XMT_STATUS);
	if (G_XSTAT_XS(status) == K_XS_DISABLED)
	    break;
	cfe_usleep(100);
	}
    if (i == 0)
	xprintf("%s: cannot clear tx DMA\n", bcm4401_devname(sc));

    WRITECSR(sc, R_RCV_CONTROL, 0);
    for (i = 1000; i > 0; i--) {
	status = READCSR(sc, R_RCV_STATUS);
	if (G_RSTAT_RS(status) == K_RS_DISABLED)
	    break;
	cfe_usleep(100);
	}
    if (i == 0)
	xprintf("%s: cannot clear rx DMA\n", bcm4401_devname(sc));

    status = READCSR(sc, R_INT_STATUS);
    WRITECSR(sc, R_INT_STATUS, status);
#if IPOLL
    cfe_free_irq(sc->irq, 0);
#endif

    /* Leave the mii inteface enabled */
    mii_enable(sc);
}


/* Silicon Backplane initialization for the MAC core. */

static void
bcm4401_coreinit(bcm4401_softc *sc)
{
    /* Enable the core by enabling the core clocks and then clearing
       RESET.  The backplane mapping registers have been initialized
       from the SPROM, but a more paranoid implementation would
       reconfigure at this point. */
    WRITECSR(sc, R_SBTMSTATELOW, M_SBTS_RS | M_SBTS_CE | M_SBTS_FC);
    (void)READCSR(sc, R_SBTMSTATELOW);   /* push */
    cfe_usleep(100);

    /* "PR3158 workaround - not fixed in any chip yet" */
    if ((READCSR(sc, R_SBTMSTATEHI) & M_SBTS_SE) != 0) {
	WRITECSR(sc, R_SBTMSTATEHI, 0);
	}
    if ((READCSR(sc, R_SBIMSTATE) & (M_SBIS_IE | M_SBIS_TO)) != 0) {
	uint32_t sbis;
	sbis = READCSR(sc, R_SBIMSTATE);
	sbis &= ~(M_SBIS_IE | M_SBIS_TO);
	WRITECSR(sc, R_SBIMSTATE, sbis);
	}
    /* End of workaround */

    WRITECSR(sc, R_SBTMSTATELOW, M_SBTS_CE | M_SBTS_FC);
    (void)READCSR(sc, R_SBTMSTATELOW);   /* push */
    cfe_usleep(100);
    WRITECSR(sc, R_SBTMSTATELOW, M_SBTS_CE);
    (void)READCSR(sc, R_SBTMSTATELOW);   /* push */
    cfe_usleep(100);
}


/* Declarations for CFE Device Driver Interface routines */

static int bcm4401_ether_open(cfe_devctx_t *ctx);
static int bcm4401_ether_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm4401_ether_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int bcm4401_ether_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm4401_ether_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int bcm4401_ether_close(cfe_devctx_t *ctx);
static void bcm4401_ether_poll(cfe_devctx_t *ctx, int64_t ticks);
static void bcm4401_ether_reset(void *softc);


/* CFE Device Driver dispatch structure */

const static cfe_devdisp_t bcm4401_ether_dispatch = {
    bcm4401_ether_open,
    bcm4401_ether_read,
    bcm4401_ether_inpstat,
    bcm4401_ether_write,
    bcm4401_ether_ioctl,
    bcm4401_ether_close,
    bcm4401_ether_poll,
    bcm4401_ether_reset
};


#if CFG_PCI
/* CFE Device Driver probe functions for PCI NIC (bcm4401) */

/* The following are for the cores as packaged in the 4401. */

#define SB4401_ENET_BASE        0x18000000
#define SB4401_CODEC_BASE       0x18001000    /* bond-out on 4402 only */
#define SB4401_PCI_BASE         0x18002000

#define K_SB4401INT_PCI         (1<<0)
#define K_SB4401INT_ENET_MAC    (1<<1)
#define K_SB4401INT_CODEC       (1<<2)

const cfe_driver_t bcm4401drv = {
    "BCM4401 Ethernet",
    "eth",
    CFE_DEV_NETWORK,
    &bcm4401_ether_dispatch,
    bcm4401_ether_probe
};

static void
bcm4401_pciconfig(bcm4401_softc *sc)
{
    uint32_t oldsb;
    uint32_t idhigh;
    uint32_t xlat;

    oldsb = pci_conf_read(sc->tag, PCI_PCIBAR0WINDOW_REG);
    pci_conf_write(sc->tag, PCI_PCIBAR0WINDOW_REG, SB4401_PCI_BASE);
    (void)pci_conf_read(sc->tag, PCI_PCIBAR0WINDOW_REG);   /* push */

    idhigh = READCSR(sc, R_SBIDHIGH);
    if (G_SBID_RV(idhigh) < 6) {
	/* A0 and A1 parts */
	WRITECSR(sc, R_SBINTVEC, V_SBINT_MK(K_SB4401INT_ENET_MAC));
	}
    else {
	/* B0 parts (PCI Rev 2.3 support) */
	uint32_t mask;

	mask = pci_conf_read(sc->tag, PCI_PCIINTMASK_REG);
	mask |= V_SBINT_IM(K_SB4401INT_ENET_MAC);
	pci_conf_write(sc->tag, PCI_PCIINTMASK_REG, mask);
	}

    xlat = READCSR(sc, R_SB_TO_PCI_TRANSLATION2);
    xlat |= (M_SBXLAT_PE | M_SBXLAT_WB);
    WRITECSR(sc, R_SB_TO_PCI_TRANSLATION2, xlat);

    (void)READCSR(sc, R_SB_TO_PCI_TRANSLATION2);           /* push */

    pci_conf_write(sc->tag, PCI_PCIBAR0WINDOW_REG, oldsb);
    (void)pci_conf_read(sc->tag, PCI_PCIBAR0WINDOW_REG);   /* push */
}

static int
bcm4401_ether_attach(cfe_driver_t *drv, pcitag_t tag)
{
    bcm4401_softc *sc;
    uint32_t device;
    uint32_t class;
    phys_addr_t pa;
    uint8_t sprom[SPROM_SIZE];
    char descr[100];

    device = pci_conf_read(tag, PCI_ID_REG);
    class = pci_conf_read(tag, PCI_CLASS_REG);

    /* Use memory space for the CSRs */
    pci_map_mem(tag, PCI_MAPREG(0), PCI_MATCH_BITS, &pa);

    sc = (bcm4401_softc *) KMALLOC(sizeof(bcm4401_softc), 0);

    if (sc == NULL) {
	xprintf("BCM4401: No memory to complete probe\n");
	return 0;
	}
    memset(sc, 0, sizeof(*sc));

    sc->phys_to_dma = phys_to_pci;
    sc->dma_to_phys = pci_to_phys;

    sc->membase = (uint32_t)pa;
    sc->irq = pci_conf_read(tag, PCI_BPARAM_INTERRUPT_REG) & 0xFF;
    sc->tag = tag;

    sc->device = PCI_PRODUCT(device);
    sc->revision = PCI_REVISION(class);
    sc->devctx = NULL;

    sc->linkspeed = ETHER_SPEED_AUTO;    /* only autonegotiation for now */
    sc->loopback = ETHER_LOOPBACK_OFF;

    /* SOC core initialization. */
    bcm4401_coreinit(sc);

    bcm4401_pciconfig(sc);

    mii_enable(sc);

    bcm4401_init(sc);

    sprom_read_all(sc, sprom);
    if (B44_DEBUG) sprom_dump(sprom);

    /* Use the address in EEPROM.  */
    memcpy(sc->hwaddr, &sprom[SPROM_MAC_ADDR], ENET_ADDR_LEN);
    sc->phy_addr = sprom[SPROM_PHY_ADDR] & 0x1F;

    sc->state = eth_state_uninit;

    xsprintf(descr, "%s at 0x%X (%a)",
	     drv->drv_description, sc->membase, sc->hwaddr);

    cfe_attach(drv, sc, NULL, descr);
    return 1;
}

static void
bcm4401_ether_probe(cfe_driver_t *drv,
		    unsigned long probe_a, unsigned long probe_b, 
		    void *probe_ptr)
{
    int index;

    index = 0;
    for (;;) {
	pcitag_t tag;
	pcireg_t device;

	if (pci_find_class(PCI_CLASS_NETWORK, index, &tag) != 0)
	    break;

	index++;

	device = pci_conf_read(tag, PCI_ID_REG);
	if (PCI_VENDOR(device) == K_PCI_VENDOR_BROADCOM) {
	    switch (PCI_PRODUCT(device)) {
		case K_PCI_ID_BCM4401:
	        case K_PCI_ID_BCM4401_B:
		case K_PCI_ID_BCM4402:
		    bcm4401_ether_attach(drv, tag);
		    break;
		default:
		    break;
		}
	    }
	}
}
#endif /* CFG_PCI */


/* CFE Device Driver probe functions for SOC core. */

const cfe_driver_t sb_mac = {
    "BCM47xx Ethernet",
    "eth",
    CFE_DEV_NETWORK,
    &bcm4401_ether_dispatch,
    sb_ether_probe
};

static int
sb_ether_attach(cfe_driver_t *drv,
		int unit, const char *mac_addr, int phy_addr)
{
    bcm4401_softc *sc;
    phys_addr_t pa;
    uint32_t flag, id;
    char descr[100];

    /* XXX The map of enumeration space should be discovered by scanning. */
#ifdef SB_ENET1_BASE
    static const phys_addr_t enet_base[2] = {SB_ENET0_BASE, SB_ENET1_BASE};
#else
    static const phys_addr_t enet_base[1] = {SB_ENET0_BASE};
#endif

    /* Use memory space for the CSRs */
    pa = enet_base[unit];

    sc = (bcm4401_softc *) KMALLOC(sizeof(bcm4401_softc), CACHE_ALIGN);
    if (sc == NULL) {
	xprintf("BCM4xxx MAC: No memory to complete probe\n");
	return 0;
	}
    memset(sc, 0, sizeof(*sc));

    sc->membase = (uint32_t)pa;

    sc->phys_to_dma = phys_to_sb;
    sc->dma_to_phys = sb_to_phys;

    flag = READCSR(sc, R_SBTPSFLAG);
    sc->irq = G_SBTSF_FN(flag);
    sc->tag = 0;
    id = READCSR(sc, R_SBIDHIGH);
    sc->device = G_SBID_CR(id);
    sc->revision = G_SBID_RV(id);
    sc->devctx = NULL;

    sc->linkspeed = ETHER_SPEED_AUTO;    /* select autonegotiation */
    sc->loopback = ETHER_LOOPBACK_OFF;

    /* Enable the core by enabling the core clocks and then clearing
       RESET.  The backplane mapping registers are fixed but vary
       among the various 47xx SOCs. */
    WRITECSR(sc, R_SBTMSTATELOW, M_SBTS_RS | M_SBTS_CE | M_SBTS_FC);
    (void)READCSR(sc, R_SBTMSTATELOW);   /* push */
    cfe_usleep(100);

    /* "PR3158 workaround - not fixed in any chip yet" */
    if ((READCSR(sc, R_SBTMSTATEHI) & M_SBTS_SE) != 0) {
	WRITECSR(sc, R_SBTMSTATEHI, 0);
	}
    if ((READCSR(sc, R_SBIMSTATE) & (M_SBIS_IE | M_SBIS_TO)) != 0) {
	uint32_t sbis;
	sbis = READCSR(sc, R_SBIMSTATE);
	sbis &= ~(M_SBIS_IE | M_SBIS_TO);
	WRITECSR(sc, R_SBIMSTATE, sbis);
	}
    /* End of workaround */

    WRITECSR(sc, R_SBTMSTATELOW, M_SBTS_CE | M_SBTS_FC);
    (void)READCSR(sc, R_SBTMSTATELOW);   /* push */
    cfe_usleep(100);
    WRITECSR(sc, R_SBTMSTATELOW, M_SBTS_CE);
    (void)READCSR(sc, R_SBTMSTATELOW);   /* push */
    cfe_usleep(100);

    bcm4401_coreinit(sc);

    mii_enable(sc);

    bcm4401_init(sc);

    /* XXX How should 47xx SOCs get their MAC addresses? */
    (void)sprom_read_all;
    (void)sprom_dump;
    if (mac_addr) {
	enet_parse_hwaddr(mac_addr, sc->hwaddr);
	}
    else {
	sc->hwaddr[0] = 0x02; sc->hwaddr[1] = 0x00; sc->hwaddr[2] = 0x00;
	sc->hwaddr[3] = 0x47; sc->hwaddr[4] = 0x10; sc->hwaddr[5] = 0x10+unit;
	}
    sc->phy_addr = phy_addr;

    sc->state = eth_state_uninit;

    xsprintf(descr, "%s at 0x%X (%a)",
	     drv->drv_description, sc->membase, sc->hwaddr);

//    cfe_attach(drv, sc, NULL, descr);
    gsc = sc;

    sc->inpkts = sc->outpkts = 0;
    sc->interrupts = 0;
    sc->rx_interrupts = sc->tx_interrupts = 0;

    bcm4401_start(sc);


    return 1;
}

/*
 *  Probe arguments:
 *         probe_a - index of the MAC to probe
 *         probe_b - PHY address for MII
 *         probe_ptr - string pointer to hardware address for this
 *                     MAC, in the form xx:xx:xx:xx:xx:xx.
 */


static void
sb_ether_probe(cfe_driver_t *drv,
		    unsigned long probe_a, unsigned long probe_b, 
		    void *probe_ptr)
{
    int index = probe_a;
    int phy_addr = probe_b;
    const char *mac_addr = probe_ptr;

    if (index == 0 || index == 1)
	sb_ether_attach(drv, index, mac_addr, phy_addr);
}


/* The functions below are called via the dispatch vector for the 4401. */

static int
bcm4401_ether_open(cfe_devctx_t *ctx)
{
    bcm4401_softc *sc = ctx->dev_softc;

    if (sc->state == eth_state_on)
	bcm4401_stop(sc);

    sc->devctx = ctx;

    sc->inpkts = sc->outpkts = 0;
    sc->interrupts = 0;
    sc->rx_interrupts = sc->tx_interrupts = 0;

    bcm4401_start(sc);

#if XPOLL
    bcm4401_isr(sc);
#endif

    return 0;
}

static int
bcm4401_ether_read(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    bcm4401_softc *sc = ctx->dev_softc;
    eth_pkt_t *pkt;
    int blen;

#if XPOLL
    bcm4401_isr(sc);
#endif

    if (sc->state != eth_state_on) return -1;

    CS_ENTER(sc);
    pkt = (eth_pkt_t *) q_deqnext(&(sc->rxqueue));
    CS_EXIT(sc);

    if (pkt == NULL) {
	buffer->buf_retlen = 0;
	return 0;
	}

    blen = buffer->buf_length;
    if (blen > pkt->length) blen = pkt->length;

    CACHE_DMA_INVAL(pkt->buffer+PKTBUF_RX_OFFSET, blen);
    hs_memcpy_to_hs(buffer->buf_ptr, pkt->buffer+PKTBUF_RX_OFFSET, blen);
    buffer->buf_retlen = blen;

    eth_free_pkt(sc, pkt);
    bcm4401_fillrxring(sc);

#if XPOLL
    bcm4401_isr(sc);
#endif

    return 0;
}

static int
bcm4401_ether_inpstat(cfe_devctx_t *ctx, iocb_inpstat_t *inpstat)
{
    bcm4401_softc *sc = ctx->dev_softc;

#if XPOLL
    bcm4401_isr(sc);
#endif

    if (sc->state != eth_state_on) return -1;

    /* We avoid an interlock here because the result is a hint and an
       interrupt cannot turn a non-empty queue into an empty one. */
    inpstat->inp_status = (q_isempty(&(sc->rxqueue))) ? 0 : 1;

    return 0;
}

static int
bcm4401_ether_write(cfe_devctx_t *ctx, iocb_buffer_t *buffer)
{
    bcm4401_softc *sc = ctx->dev_softc;
    eth_pkt_t *pkt;
    int blen;

#if XPOLL
    bcm4401_isr(sc);
#endif

    if (sc->state != eth_state_on) return -1;

    pkt = eth_alloc_pkt(sc);
    if (!pkt) return CFE_ERR_NOMEM;

    blen = buffer->buf_length;
    if (blen > pkt->length) blen = pkt->length;

    hs_memcpy_from_hs(pkt->buffer, buffer->buf_ptr, blen);
    pkt->length = blen;
    CACHE_DMA_SYNC(pkt->buffer, blen);

    if (bcm4401_transmit(sc, pkt) != 0) {
	eth_free_pkt(sc,pkt);
	return CFE_ERR_IOERR;
	}

#if XPOLL
    bcm4401_isr(sc);
#endif

    return 0;
}

static int
bcm4401_ether_ioctl(cfe_devctx_t *ctx, iocb_buffer_t *buffer) 
{
    bcm4401_softc *sc = ctx->dev_softc;
    int   mode;
    int   speed;

    switch ((int)buffer->buf_ioctlcmd) {
	case IOCTL_ETHER_GETHWADDR:
	    hs_memcpy_to_hs(buffer->buf_ptr, sc->hwaddr, sizeof(sc->hwaddr));
	    return 0;

	case IOCTL_ETHER_SETHWADDR:
	    return -1;    /* not supported */

	case IOCTL_ETHER_GETSPEED:
	    speed = sc->linkspeed;
	    hs_memcpy_to_hs(buffer->buf_ptr, &speed, sizeof(speed));
	    return 0;

	case IOCTL_ETHER_SETSPEED:
	    hs_memcpy_from_hs(&speed,buffer->buf_ptr, sizeof(speed));
	    bcm4401_setspeed(sc, speed);
	    return -1;    /* not supported yet */

	case IOCTL_ETHER_GETLINK:
	    speed = sc->linkspeed;
	    hs_memcpy_to_hs(buffer->buf_ptr, &speed, sizeof(speed));
	    return 0;

	case IOCTL_ETHER_GETLOOPBACK:
	    speed = sc->loopback;
	    hs_memcpy_to_hs(buffer->buf_ptr, &speed, sizeof(speed));
	    return 0;

	case IOCTL_ETHER_SETLOOPBACK:
	    hs_memcpy_from_hs(&mode,buffer->buf_ptr, sizeof(mode));
	    sc->loopback = ETHER_LOOPBACK_OFF;  /* default */
	    if (mode == ETHER_LOOPBACK_INT || mode == ETHER_LOOPBACK_EXT) {
		bcm4401_setloopback(sc, mode);
		}
	    return -1;    /* not supported yet */

	default:
	    return -1;
	}
}

static int
bcm4401_ether_close(cfe_devctx_t *ctx)
{
    bcm4401_softc *sc = ctx->dev_softc;

    sc->state = eth_state_off;
    bcm4401_stop(sc);

    xprintf("%s: %d sent, %d received, %d interrupts\n",
	    bcm4401_devname(sc), sc->outpkts, sc->inpkts, sc->interrupts);
    if (IPOLL) {
	xprintf("  %d tx interrupts, %d rx interrupts\n",
		sc->tx_interrupts, sc->rx_interrupts);
	}

    /* resynchronize descriptor rings */
    bcm4401_resetrings(sc);

    sc->devctx = NULL;
#if 1 /* XXX Redo partitioning among hwinit, start and stop */
    sc->state = eth_state_uninit;
#endif
    return 0;
}

static void
bcm4401_ether_poll(cfe_devctx_t *ctx, int64_t ticks)
{
    bcm4401_softc *sc = (bcm4401_softc *)ctx->dev_softc;
    uint16_t phy_isr;

    /* The PHY Interrupt register appears to work as claimed with
       respect to link status changes, but I've had no luck clearing
       the MIIInt bit in the EnetIntStatus register.  We therefore use
       polling but reduce the frequency since reading MII registers is
       expensive.  */
    if (sc->slow_poll) {
	sc->slow_poll = 0;
	if (sc->phy_interrupt) {
	    phy_isr = mii_interrupt(sc);
	    if ((phy_isr & (M_PHYINT_LC | M_PHYINT_SP | M_PHYINT_DC)) != 0) {
		mii_autonegotiate(sc);
		bcm4401_set_linkspeed(sc);
		}
	    }
	}
}

static void
bcm4401_ether_reset(void *softc)
{
    bcm4401_softc *sc = (bcm4401_softc *)softc;

    /* Turn off the Ethernet interface. */
    if (sc->state == eth_state_on)
	bcm4401_stop(sc);
    bcm4401_reset(sc);

    sc->state = eth_state_uninit;
}



int
cfe_output(int buf_length, char *buf_ptr)
{
    bcm4401_softc *sc = gsc;
    eth_pkt_t *pkt;
    int blen;

#if XPOLL
    bcm4401_isr(sc);
#endif

    if (sc->state != eth_state_on) return -1;

    pkt = eth_alloc_pkt(sc);
    if (!pkt) return CFE_ERR_NOMEM;

    blen = buf_length;
    if (blen > pkt->length) blen = pkt->length;

    hs_memcpy_from_hs(pkt->buffer, buf_ptr, blen);
    pkt->length = blen;
    CACHE_DMA_SYNC(pkt->buffer, blen);

    if (bcm4401_transmit(sc, pkt) != 0) {
	eth_free_pkt(sc,pkt);
	return CFE_ERR_IOERR;
	}

#if XPOLL
    bcm4401_isr(sc);
#endif

    return 0;
}

int getchipid()
{
uint32_t chipid;

    chipid = *(volatile uint32_t *)PHYS_TO_K1(SB_CHIPC_BASE);
    return chipid & 0xffff;
}

int isbcm5354()
{

    if (getchipid() == 0x5354)
      return 1;
    else
      return 0;
}

int isbcm5350()
{

    if (getchipid() == 0x5350)
      return 1;
    else
      return 0;
}

int isbcm535x()
{

    if ((getchipid() & 0xfff0) == 0x5350)
      return 1;
    else
      return 0;
}

void cfe_ether_init(char *macaddr)
{
cfe_driver_t dev;

    if (isbcm535x())
        sb_ether_probe(&dev, 0, 0x1e, macaddr);
    else
        sb_ether_probe(&dev, 1, 0, macaddr);
}
