/*
 * Copyright (c) 2018 Hiroki Mori. All rights reserved.
 */

#include <string.h>

#include "lwip/init.h"

#include "lwip/debug.h"

#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"

#include "lwip/stats.h"

#include "lwip/ip.h"
#include "lwip/ip4_frag.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"

#include "lwip/dns.h"
#include "lwip/dhcp.h"

//#define	NETDEBUG

unsigned char debug_flags;

extern  struct netif netif;

err_t ethernetif_init(struct netif *netif);
err_t ethernet_input(struct pbuf *p, struct netif *netif);

ip4_addr_t ipaddr, netmask, gw, dnsserver, dnsres;

static struct udp_pcb *udpecho_raw_pcb;
static struct udp_pcb *udpsntp_raw_pcb;

char udpbuff[1024];
char tcpbuff[1024*8];
int tcplen;
int tcpoff;

static void
udpecho_raw_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                 const ip_addr_t *addr, u16_t port)
{
  LWIP_UNUSED_ARG(arg);
  if (p != NULL) {
      pbuf_copy_partial(p, udpbuff, p->tot_len, 0);
      udpbuff[p->tot_len] = '\0';
    /* free the pbuf */
    pbuf_free(p);
  }
}

static struct tcp_pcb *tcphttp_raw_pcb;
int tcpstat;

static err_t
http_recv(void *arg, struct tcp_pcb *pcb,
                  struct pbuf *pbuf, err_t err)
{
  if (pbuf != NULL) {
    if (tcplen == 0)
      tcpoff = 0;
    pbuf_copy_partial(pbuf, tcpbuff+tcpoff+tcplen, pbuf->tot_len, 0);
    tcplen += pbuf->tot_len;
    tcp_recved(pcb, pbuf->tot_len);
    pbuf_free(pbuf);
  } else {
    tcpstat = 2;
  }
  return ERR_OK;
}

static err_t
http_sent (void *arg, struct tcp_pcb *pcb, u16_t len)
{
  return ERR_OK;
}

static err_t
http_poll(void *arg, struct tcp_pcb *pcb)
{
  tcp_abort(pcb);
  tcpstat = 3;
  print("tcp_abort");
  return ERR_OK;
}

static void
http_err(void *arg, err_t err)
{
  tcpstat = 3;
}

static err_t
http_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
  tcplen = 0;
  tcpoff = 0;
  tcpstat = 1;
  return ERR_OK;
}

void
my_tcp_write(char *buf, int len)
{
  err_t err = ERR_OK;
  len = (u16_t)LWIP_MIN(len, tcp_sndbuf(tcphttp_raw_pcb));
  err = tcp_write(tcphttp_raw_pcb, buf, len, TCP_WRITE_FLAG_COPY);
  tcp_output(tcphttp_raw_pcb);
}

void
my_tcp_close()
{
  tcp_close(tcphttp_raw_pcb);
}

void
tcphttp_raw_init(int disaddr, int disport)
{
static ip4_addr_t addr;

  ip4_addr_set_u32(&addr, lwip_htonl(disaddr));

  tcphttp_raw_pcb = tcp_new();
  if (tcphttp_raw_pcb != NULL) {
//    tcp_arg(tcphttp_raw_pcb, NULL);
    tcp_recv(tcphttp_raw_pcb, http_recv);
    tcp_sent(tcphttp_raw_pcb, http_sent);
    tcp_err(tcphttp_raw_pcb, http_err);
    tcp_poll(tcphttp_raw_pcb, http_poll, 60);
    tcp_connect(tcphttp_raw_pcb, &addr, disport, http_connected);
    tcpstat = 0;
  }
}

int netstat = 0;

void net_poll()
{

	if (netstat == 1) {
		sys_check_timeouts();
		doque();
	}
}

int dnsstat;
int resolvip;

static void
dns_found(const char* hostname, const ip_addr_t *ipaddr, void *arg)
{
  LWIP_UNUSED_ARG(arg);

  if (ipaddr != 0) {
    resolvip = ip_addr_get_ip4_u32(ipaddr);
    dnsstat = 1;
  } else {
    dnsstat = 2;
  }
}

void net_init(int use_dhcp)
{
long *lptr;
err_t err;
int i;

	lwip_init();

	if (use_dhcp)
		netif_add(&netif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask),
		   ip_2_ip4(&gw), NULL, ethernetif_init, ethernet_input);
	else
		netif_add(&netif, &ipaddr, &netmask, &gw, NULL, ethernetif_init,
 		    ethernet_input);

	netif_set_link_up(&netif);

	netif_set_default(&netif);
	netif_set_up(&netif);   /* send broadcast arp packet */

	netstat = 1;

	if (use_dhcp) {
		err = dhcp_start(&netif);
		if (err != ERR_OK) {
			print("dhcp error\n");
			netstat = 0;
		} else {
			for (i = 0; i < 1000; ++i) {
				delay_ms(100);
				if (dhcp_supplied_address(&netif))
					break;
			}
			if (netif.ip_addr.addr != 0) {
#ifdef NETDEBUG
				xprintf("IP address : %d.%d.%d.%d\n",
				    (netif.ip_addr.addr >> 24) & 0xff,
				    (netif.ip_addr.addr >> 16) & 0xff,
				    (netif.ip_addr.addr >> 8) & 0xff,
				    netif.ip_addr.addr & 0xff);
#endif
			} else {
				print("dhcp can't get address\n");
				netstat = 0;
			}
		}
	} else {
		dns_setserver(0, &dnsserver);
	}

	udpsntp_raw_pcb = NULL;
}

void net_start(int myaddr, int mymask, int mygw, int mydns)
{
	ip4_addr_set_u32(&ipaddr, lwip_htonl(myaddr));
	ip4_addr_set_u32(&netmask, lwip_htonl(mymask));
	ip4_addr_set_u32(&gw, lwip_htonl(mygw));
	ip4_addr_set_u32(&dnsserver, lwip_htonl(mydns));

	net_init(0);
}

void net_startdhcp()
{
	IP4_ADDR(&ipaddr, 0,0,0,0);
	IP4_ADDR(&netmask, 0,0,0,0);
	IP4_ADDR(&gw, 0,0,0,0);

	net_init(1);
}

int getmyaddress()
{
	return lwip_htonl(netif.ip_addr.addr);
}

void
rtl_udp_init()
{
  udpecho_raw_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
}

void
rtl_udp_bind(int port)
{
  udpbuff[0] = '\0';
  if (udpecho_raw_pcb != NULL) {
    err_t err;

    err = udp_bind(udpecho_raw_pcb, IP_ANY_TYPE, port);
    if (err == ERR_OK) {
      udp_recv(udpecho_raw_pcb, udpecho_raw_recv, NULL);
    } else {
      /* abort? output diagnostic? */
    }
  } else {
    /* abort? output diagnostic? */
  }
}

int
rtl_udp_recv(char *buf, int len)
{
int rlen, udplen;

	rlen = 0;

	if (udpbuff[0] != 0) {
		cli();
		udplen = strlen(udpbuff);
		rlen = udplen > len ? len : udplen;
		memcpy(buf, udpbuff, rlen);
		udpbuff[0] = '\0';
		sti();
	}
	return rlen;
}

void
rtl_udp_send(int addr, int port, char *buf, int len)
{
static ip4_addr_t distaddr;

	if (udpecho_raw_pcb != NULL) {
		ip4_addr_set_u32(&distaddr, lwip_htonl(addr));
		struct pbuf* b = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_POOL);
		memcpy(b->payload, buf, len);
// UDP send must be live gateway. Because of gateware arp resolve at UDP send
// on lwip 2.0.3.
		udp_sendto(udpecho_raw_pcb, b, &distaddr, port);
// same as above
//		udp_connect(udpecho_raw_pcb, &distaddr, port);
//		udp_send(udpecho_raw_pcb, b);
//		udp_disconnect(udpecho_raw_pcb);
		pbuf_free(b);
	}
}


extern unsigned long starttime;

static void
udpsntp_raw_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                 const ip_addr_t *addr, u16_t port)
{
unsigned int timestamp;
unsigned char buf[48];

	LWIP_UNUSED_ARG(arg);
	if (p != NULL) {
		if (p->tot_len == sizeof(buf)) {
			pbuf_copy_partial(p, buf, p->tot_len, 0);
			timestamp = buf[40] << 24 | buf[41] << 16 |
			    buf[42] << 8 | buf[43];
			timestamp -= 2208988800UL;
			reset_counter();
			starttime = timestamp;
		}
 		/* free the pbuf */
		pbuf_free(p);

		udp_remove(udpsntp_raw_pcb);
		udpsntp_raw_pcb == NULL;
	}
}

void sntp(int addr)
{
int port;
static ip4_addr_t distaddr;

	if (udpsntp_raw_pcb != NULL) {
		udp_remove(udpsntp_raw_pcb);
	}

	port = 123;
	udpsntp_raw_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
	udp_bind(udpsntp_raw_pcb, IP_ANY_TYPE, port);
	udp_recv(udpsntp_raw_pcb, udpsntp_raw_recv, NULL);

	unsigned char msg[48]={0,0,0,0,0,0,0,0,0};

	msg[0] = 4 << 3 | 3;

	ip4_addr_set_u32(&distaddr, lwip_htonl(addr));
	struct pbuf* b = pbuf_alloc(PBUF_TRANSPORT, sizeof(msg), PBUF_POOL);
	memcpy(b->payload, msg, sizeof(msg));
	udp_sendto(udpsntp_raw_pcb, b, &distaddr, port);
	pbuf_free(b);
}

int
lookup(char *host, int *addr)
{
err_t err;
int res;

	res = 0;
	*addr = 0;
	dnsstat = 0;
	err = dns_gethostbyname(host,
	    &dnsres, dns_found, NULL);
	if (err == ERR_OK) {
		*addr = lwip_htonl(ip_addr_get_ip4_u32(&dnsres));
		res = 1;
	} else {
		while(dnsstat == 0)
			delay_ms(10);
		if (dnsstat == 1) {
			*addr = lwip_htonl(resolvip);
			res = 1;
		}
	}
	return res;
}

int
http_connect(int addr, int port, char *header)
{
        tcphttp_raw_init(addr, port);
        while(tcpstat == 0)
                delay_ms(10);
        if (tcpstat == 1) {
		my_tcp_write(header, strlen(header));
                return 1;
        }
        return 0;
}

int
http_read(char *buf, int len)
{
int rlen = 0;
int i;

	if (tcplen != 0) {
		cli();
		rlen = tcplen > len ? len : tcplen;
		memcpy(buf, tcpbuff + tcpoff, rlen);
		tcplen -= rlen;
		tcpoff += rlen;
		sti();
	} else if (tcpstat >= 2) {
		rlen = -1;
	}
	return rlen;
}

void
http_close()
{
	tcp_close(tcphttp_raw_pcb);
}
