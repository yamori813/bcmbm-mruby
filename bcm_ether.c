/*
 * Copyright (c) 2018 Hiroki Mori. All rights reserved.
 */

#include "lwip/init.h"

#include "lwip/debug.h"

#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"

struct netif netif;

extern ip4_addr_t ipaddr, netmask, gw, dnsserver, dnsres;

struct ethernetif {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
};

extern int netstat;

#define INQEULEN 32
 
struct pbuf *inque[INQEULEN];
int inquestart;
int inqueend;

eninque(struct pbuf *p)
{
	inque[inqueend] = p;
	++inqueend;
	if (inqueend == INQEULEN)
		inqueend = 0;
}

doque()
{
err_t err;
  
	if(inquestart != inqueend) {
		while (inquestart != inqueend) {
			err = netif.input(inque[inquestart], &netif);
			if (err != ERR_OK) {
				pbuf_free(inque[inquestart]);                  
			}
			++inquestart;
			if (inquestart == INQEULEN)
				inquestart = 0;
		}
	}
}

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
	cfe_output(p->tot_len, p->payload);
	return ERR_OK;
}

err_t
ethernetif_init(struct netif *netif)
{
struct ethernetif *ethernetif;
char *macptr;

	ethernetif = mem_malloc(sizeof(struct ethernetif));
	if (ethernetif == NULL) {
		LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory?n"));
		return ERR_MEM;
	}

	inquestart = 0;      
	inqueend = 0;

	netif->state = ethernetif;
	netif->name[0] = 'b';
	netif->name[1] = 'e';

	netif->output = etharp_output;
	netif->output_ip6 = ethip6_output;
	netif->linkoutput = low_level_output;

	macptr = nvram_get("et0macaddr");
	if (macptr == NULL)
		macptr = "12:34:56:78:9a:bc";

	netif->hwaddr_len = ETHARP_HWADDR_LEN;
	enet_parse_hwaddr(macptr,  netif->hwaddr);

	netif->mtu = 1500;
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
	    NETIF_FLAG_LINK_UP;

	cfe_ether_init(macptr);

	return ERR_OK;
}

void net_rcv(char *data, int len)
{
struct pbuf *p;
int i;

	p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
	pbuf_take(p, data+32, len);
/*
	for (i = 0; i < len; ++i)
		xprintf("%02x ", data[i+32]);
	xprintf("\n");
*/
	eninque(p);
}
