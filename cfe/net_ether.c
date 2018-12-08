/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Ethernet Datalink			File: net_ether.c
    *  
    *  This module provides a simple datalink (LLC1) interface 
    *  capable of demultiplexing standard DIX-style Ethernet packets.
    *  
    *  Author:  Mitch Lichtenberg
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
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

#include "net_enet.h"
#include "net_ebuf.h"
#include "net_ether.h"


/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define ETH_MAX_PORTS	4
#define ETH_MAX_BUFFERS	8

/*  *********************************************************************
    *  Types
    ********************************************************************* */

typedef struct ether_port_s {
    int ep_dev;
    uint8_t ep_proto[8];
    int ep_ptype;
    int ep_mtu;
    int (*ep_rxcallback)(ebuf_t *buf,void *ref);
    void *ep_ref;
} ether_port_t;

struct ether_info_s {
    ether_port_t *eth_ports;
    queue_t eth_freelist;
    uint8_t eth_hwaddr[6];
    int eth_devhandle;
    ebuf_t *eth_bufpool;
};

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

const uint8_t enet_broadcast[ENET_ADDR_LEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

#if NOTUSE

/*  *********************************************************************
    *  eth_open(eth,ptye,pdata,cb)
    *  
    *  Open an Ethernet portal.
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   ptype - protocol type (ETH_PTYPE_xxx)
    *  	   pdata - protocol data (two bytes for DIX protocols)
    *  	   cb - callback for receive packets
    *  	   
    *  Return value:
    *  	   portal number 
    *  	   or <0 if error occured
    ********************************************************************* */

int eth_open(ether_info_t *eth,int ptype,char *pdata,
	     int (*cb)(ebuf_t *buf,void *ref),void *ref)
{
    ether_port_t *p;
    int portnum;

    p = eth->eth_ports;

    for (portnum = 0; portnum < ETH_MAX_PORTS; portnum++,p++) {
	if (p->ep_rxcallback == NULL) break;
	}
    
    if (portnum == ETH_MAX_PORTS) {
	return CFE_ERR_NOHANDLES;	/* no ports left */
	}

    switch (ptype) {
	case ETH_PTYPE_DIX:
	    p->ep_proto[0] = pdata[0];
	    p->ep_proto[1] = pdata[1];
	    p->ep_mtu = ENET_MAX_PKT - ENET_DIX_HEADER;
	    break;

	case ETH_PTYPE_802SAP:
	case ETH_PTYPE_802SNAP:
	default:
	    /*
	     * we only support DIX etypes right now.  If we ever want to support
	     * non-IP stacks (unlikely) this will need to change.
	     */
	    return CFE_ERR_UNSUPPORTED;
	}

    p->ep_ptype = ptype;
    p->ep_rxcallback = cb;
    p->ep_dev = eth->eth_devhandle;
    p->ep_ref = ref;

    return portnum;
}


/*  *********************************************************************
    *  eth_close(eth,port)
    *  
    *  Close an Ethernet portal, freeing resources allocated to it.
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   port - portal number
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void eth_close(ether_info_t *eth,int port)
{
    ether_port_t *p = &(eth->eth_ports[port]);

    p->ep_ptype = 0;
    p->ep_rxcallback = NULL;
    p->ep_dev = 0;
    memset(&(p->ep_proto[0]),0,sizeof(p->ep_proto));
}


/*  *********************************************************************
    *  eth_findport(eth,buf)
    *  
    *  Locate the portal associated with a particular Ethernet packet.
    *  Parse the packet enough to determine if it's addressed
    *  correctly and to a valid protocol, and then look up the 
    *  corresponding portal.
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   buf - ethernet buffer to check
    *  	   
    *  Return value:
    *  	   eth_port_t structure or NULL if packet should be dropped
    ********************************************************************* */

static ether_port_t *eth_findport(ether_info_t *eth,ebuf_t *buf)
{
    int idx;
    ether_port_t *p;

    /*
     * A few pre-flight checks: packets *from* multicast addresses
     * are not allowed.
     */

    if (buf->eb_ptr[6] & 1) return NULL;

    /*
     * Packets smaller than minimum size are not allowed.
     */

    if (buf->eb_length < 60) return NULL;

    /*
     * Packets with bad status are not allowed
     */

    /* XXX if (buf->eb_status != 0) return NULL; */

    /*
     * Okay, scan the port list and find the matching portal.
     */

    for (idx = 0, p = eth->eth_ports; idx < ETH_MAX_PORTS; idx++,p++) {
	if (!p->ep_rxcallback) continue; /* port not in use */

	switch (p->ep_ptype) {
	    case ETH_PTYPE_DIX:
		if ((p->ep_proto[0] == buf->eb_ptr[12]) &&
		    (p->ep_proto[1] == buf->eb_ptr[13])) {
		    ebuf_skip(buf,ENET_DIX_HEADER);
		    return p;
		    }
		break;
	    case ETH_PTYPE_802SAP:
	    case ETH_PTYPE_802SNAP:
	    default:
		break;
	    }
	}

    return NULL;
}

/*  *********************************************************************
    *  eth_poll(eth)
    *  
    *  Poll devices and process inbound packets.  If new packets arrive,
    *  call the appropriate callback routine.
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void eth_poll(ether_info_t *eth)
{
    ebuf_t *buf;
    ether_port_t *p;
    int res;

    /* XXX should this loop until all packets are processed? */

    /*
     * If no packets, just get out now
     */

    if (cfe_inpstat(eth->eth_devhandle) == 0) return;

    /*
     * get a packet from the free list
     */

    buf = (ebuf_t *) q_deqnext(&(eth->eth_freelist));
    if (!buf) return;

    /*
     * Receive network data into the packet buffer
     */

    ebuf_init_rx(buf);
    res = cfe_read(eth->eth_devhandle,PTR2HSADDR(buf->eb_ptr),ENET_MAX_PKT);

    /*
     * if receive error, get out now.
     */

    if (res <= 0) {
	q_enqueue(&(eth->eth_freelist),(queue_t *) buf);
	return;
	}

    /*
     * init the rest of the fields in the ebuf
     */

    buf->eb_length = res;
    buf->eb_status = 0;
    buf->eb_device = eth;
    buf->eb_usrdata = 0;

    /*
     * Look up the portal to receive the new packet 
     */

    p = eth_findport(eth,buf);

    /*
     * Call the callback routine if we want to keep this
     * buffer.  Otherwise, drop it on the floor
     */

    if (p) {
	buf->eb_port = p - eth->eth_ports;
	res = (*(p->ep_rxcallback))(buf,p->ep_ref);
	if (res == ETH_DROP) eth_free(buf);
	}
    else {
	eth_free(buf);
	}
}


/*  *********************************************************************
    *  eth_gethwaddr(eth,hwaddr)
    *  
    *  Obtain the hardware address of the Ethernet interface.
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   hwaddr - place to put hardware address - 6 bytes
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void eth_gethwaddr(ether_info_t *eth,uint8_t *hwaddr)
{
    memcpy(hwaddr,eth->eth_hwaddr,ENET_ADDR_LEN);
}

/*  *********************************************************************
    *  eth_sethwaddr(eth,hwaddr)
    *  
    *  Set the hardware address of the Ethernet interface.
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   hwaddr - new hardware address - 6 bytes
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void eth_sethwaddr(ether_info_t *eth,uint8_t *hwaddr)
{
    int retlen;

    memcpy(eth->eth_hwaddr,hwaddr,ENET_ADDR_LEN);
    cfe_ioctl(eth->eth_devhandle,IOCTL_ETHER_SETHWADDR,&(eth->eth_hwaddr[0]),
	      sizeof(eth->eth_hwaddr),&retlen,0);

}



/*  *********************************************************************
    *  eth_setspeed(eth,speed)
    *  
    *  Set the speed of the Ethernet interface.
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   speed - target speed (or auto for automatic)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

int eth_setspeed(ether_info_t *eth,int speed)
{
    int retlen;

    return cfe_ioctl(eth->eth_devhandle,IOCTL_ETHER_SETSPEED,
		     (uint8_t *) &speed,sizeof(speed),&retlen,0);

}

/*  *********************************************************************
    *  eth_setloopback(eth,loop)
    *  
    *  Configure loopback mode options for the Ethernet
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   loop - loopback mode to set
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

int eth_setloopback(ether_info_t *eth,int loop)
{
    int retlen;

    return cfe_ioctl(eth->eth_devhandle,IOCTL_ETHER_SETLOOPBACK,
		     (uint8_t *) &loop,sizeof(loop),&retlen,0);

}

/*  *********************************************************************
    *  eth_getspeed(eth,speed)
    *  
    *  Get the current setting for the Ethernet speed (note that this
    *  is the speed we want to achieve, not the current speed)
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   speed - pointer to int to receive speed
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

int eth_getspeed(ether_info_t *eth,int *speed)
{
    int retlen;

    return cfe_ioctl(eth->eth_devhandle,IOCTL_ETHER_GETSPEED,
		     (uint8_t *) speed,sizeof(*speed),&retlen,0);

}

/*  *********************************************************************
    *  eth_getloopback(eth,loop)
    *  
    *  Read the loopback state of the Ethernet interface
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   loop - pointer to int to receive loopback state
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

int eth_getloopback(ether_info_t *eth,int *loop)
{
    int retlen;

    return cfe_ioctl(eth->eth_devhandle,IOCTL_ETHER_GETLOOPBACK,
		     (uint8_t *) loop,sizeof(*loop),&retlen,0);

}

/*  *********************************************************************
    *  eth_send(buf,dest)
    *  
    *  Transmit a packet.
    *  
    *  Input parameters: 
    *  	   buf - ebuf structure describing packet
    *  	   dest - destination hardware address
    *  	   
    *  Return value:
    *  	   0 - no error
    *  	   else error code
    ********************************************************************* */

int eth_send(ebuf_t *buf,uint8_t *dest)
{
    ether_info_t *eth = buf->eb_device;
    ether_port_t *p = &(eth->eth_ports[buf->eb_port]);
    int res;

    switch (p->ep_ptype) {
	case ETH_PTYPE_DIX:
	    ebuf_seek(buf,-ENET_DIX_HEADER);
	    ebuf_put_bytes(buf,dest,ENET_ADDR_LEN);
	    ebuf_put_bytes(buf,eth->eth_hwaddr,ENET_ADDR_LEN);
	    ebuf_put_bytes(buf,p->ep_proto,2);
	    /* adjust pointer and add in DIX header length */
	    ebuf_prepend(buf,ENET_DIX_HEADER);
	    break;
	case ETH_PTYPE_802SAP:
	case ETH_PTYPE_802SNAP:
	default:
	    eth_free(buf);		/* should not happen */
	    return CFE_ERR_UNSUPPORTED;
	}

    res = cfe_write(p->ep_dev,PTR2HSADDR(ebuf_ptr(buf)),ebuf_length(buf));

    /* XXX - should we free buffers here? */

    return res;
}

/*  *********************************************************************
    *  eth_alloc(eth,port)
    *  
    *  Allocate an Ethernet buffer.  Ethernet buffers know what
    *  ports they are associated with, since we need to reserve
    *  space for the EThernet header, which might vary in size
    *  for DIX, 802, etc.
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   port - portal ID
    *  	   
    *  Return value:
    *  	   ebuf, or NULL if no ebufs left
    ********************************************************************* */

ebuf_t *eth_alloc(ether_info_t *eth,int port)
{
    ebuf_t *buf;
    ether_port_t *p = &(eth->eth_ports[port]);

    buf = (ebuf_t *) q_deqnext(&(eth->eth_freelist));
    if (buf == NULL) return NULL;

    buf->eb_status = 0;
    buf->eb_port = port;
    buf->eb_device = eth;
    ebuf_init_tx(buf);

    switch (p->ep_ptype) {
	case ETH_PTYPE_NONE:
	    break;
	case ETH_PTYPE_DIX:
	    ebuf_seek(buf,ENET_DIX_HEADER);
	    break;
	case ETH_PTYPE_802SAP:
	case ETH_PTYPE_802SNAP:
	default:
	    /* XXX Other ether types here */
	    break;
	}

    /*
     * 'eb_ptr' points at new data, length is cleared. 
     * We will add the length back in at send time when the
     * ethernet header is filled in.
     */
    buf->eb_length = 0;

    return buf;
}

/*  *********************************************************************
    *  eth_free(buf)
    *  
    *  Free an ebuf.
    *  
    *  Input parameters: 
    *  	   buf - ebuf to free
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void eth_free(ebuf_t *buf)
{
    ether_info_t *eth = buf->eb_device;

    q_enqueue(&(eth->eth_freelist),(queue_t *) buf);
}


/*  *********************************************************************
    *  eth_getmtu(eth,port)
    *  
    *  Return the mtu of the specified Ethernet port.  The mtu 
    *  is the maximum number of bytes you can put in the buffer,
    *  excluding the Ethernet header.
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   port - portal ID
    *  	   
    *  Return value:
    *  	   number of bytes
    ********************************************************************* */


int eth_getmtu(ether_info_t *eth,int port)
{
    ether_port_t *p = &(eth->eth_ports[port]);

    return p->ep_mtu;
}


/*  *********************************************************************
    *  eth_init(devname)
    *  
    *  Create an Ethernet context for a particular Ethernet device.
    *  
    *  Input parameters: 
    *  	   devname - device name for underlying Ethernet driver
    *  	   
    *  Return value:
    *  	   ethernet context, or NULL of it could not be created.
    ********************************************************************* */

ether_info_t *eth_init(char *devname)
{
    int idx;
    ebuf_t *buf;
    ether_info_t *eth;
    int devhandle;
    int retlen;

    /*
     * Open the device driver
     */

    devhandle = cfe_open(devname);
    if (devhandle < 0) return NULL;

    eth = KMALLOC(sizeof(ether_info_t),0);
    if (!eth) {
	cfe_close(devhandle);
	return NULL;
	}

    memset(eth,0,sizeof(ether_info_t));

    /*
     * Obtain hardware address
     */

    cfe_ioctl(devhandle,IOCTL_ETHER_GETHWADDR,&(eth->eth_hwaddr[0]),
	      sizeof(eth->eth_hwaddr),&retlen,0);

    /*
     * Allocate portal table
     */

    eth->eth_ports = KMALLOC(ETH_MAX_PORTS*sizeof(ether_port_t),0);
    if (!eth->eth_ports) {
	cfe_close(devhandle);
	KFREE(eth);
	return NULL;
	}

    memset(eth->eth_ports,0,ETH_MAX_PORTS*sizeof(ether_port_t));

    /*
     * Allocate buffer pool
     */

    eth->eth_bufpool = (ebuf_t *) KMALLOC(sizeof(ebuf_t)*ETH_MAX_BUFFERS,0);
    if (!eth->eth_bufpool) {
	cfe_close(devhandle);
	KFREE(eth->eth_ports);
	KFREE(eth);
	return NULL;
	}

    /*
     * Chain buffers onto the free list
     */

    q_init(&(eth->eth_freelist));
    buf = eth->eth_bufpool;
    for (idx = 0; idx < ETH_MAX_BUFFERS; idx++) {
	q_enqueue(&(eth->eth_freelist),(queue_t *) buf);
	buf++;
	}

    /*
     * Remember the device handle
     */

    eth->eth_devhandle = devhandle;

    return eth;
}


/*  *********************************************************************
    *  eth_uninit(eth)
    *  
    *  Close and free up an Ethernet context
    *  
    *  Input parameters: 
    *  	   eth - ethernet context
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

void eth_uninit(ether_info_t *eth)
{
    cfe_close(eth->eth_devhandle);
    KFREE(eth->eth_bufpool);
    KFREE(eth->eth_ports);
    KFREE(eth);
}


#endif

/*  *********************************************************************
    *  Some utilities for manipulating Ethernet addresses.
    ********************************************************************* */

/*  *********************************************************************
    *  ENET_PARSE_XDIGIT(c)
    *  
    *  Parse a hex digit, returning its value
    *  
    *  Input parameters: 
    *  	   c - character
    *  	   
    *  Return value:
    *  	   hex value, or -1 if invalid
    ********************************************************************* */

static int enet_parse_xdigit(char c)
{
    int digit;

    if ((c >= '0') && (c <= '9'))      digit = c - '0';
    else if ((c >= 'a') && (c <= 'f')) digit = c - 'a' + 10;
    else if ((c >= 'A') && (c <= 'F')) digit = c - 'A' + 10;
    else                               digit = -1;

    return digit;
}

/*  *********************************************************************
    *  ENET_PARSE_HWADDR(str,hwaddr)
    *  
    *  Convert a string in the form xx:xx:xx:xx:xx:xx into a 6-byte
    *  Ethernet address.
    *  
    *  Input parameters: 
    *  	   str - string
    *  	   hwaddr - pointer to hardware address
    *  	   
    *  Return value:
    *  	   0 if ok, else -1
    ********************************************************************* */

int enet_parse_hwaddr(const char *str, uint8_t *hwaddr)
{
    int digit1, digit2;
    int idx = ENET_ADDR_LEN;

    while (*str && (idx > 0)) {
	digit1 = enet_parse_xdigit(*str);
	if (digit1 < 0) return -1;
	str++;
	if (!*str) return -1;

	if ((*str == ':') || (*str == '-')) {
	    digit2 = digit1;
	    digit1 = 0;
	    }
	else {
	    digit2 = enet_parse_xdigit(*str);
	    if (digit2 < 0) return -1;
	    str++;
	    }

	*hwaddr++ = (digit1 << 4) | digit2;
	idx--;

	if ((*str == ':') || (*str == '-'))
	    str++;
	}
    return 0;
}

#if NOTUSE
/*  *********************************************************************
    *  ENET_INCR_HWADDR(hwaddr,incr)
    *  
    *  Increment a 6-byte Ethernet hardware address, with carries
    *  
    *  Input parameters: 
    *  	   hwaddr - pointer to hardware address
    *      incr - desired increment
    *  	   
    *  Return value:
    *  	   none
    ********************************************************************* */

void enet_incr_hwaddr(uint8_t *hwaddr, unsigned incr)
{
    int idx;
    int carry;

    idx = 5;
    carry = incr;
    while (idx >= 0 && carry != 0) {
	unsigned sum = hwaddr[idx] + carry;

	hwaddr[idx] = sum & 0xFF;
	carry = sum >> 8;
	idx--;
	}
}
#endif
