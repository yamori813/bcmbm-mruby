#
#
#

NEWLIB=newlib-3.0.0.20180831

CC=mips-gcc
AR=mips-ar

VPATH=../src/core:../src/core/ipv4:../src/core/ipv6:../src/netif

CROSS_CFLAGS = -Iinclude -I$(SRC)/include -I../../$(NEWLIB)/newlib/libc/include/

CROSS_CFLAGS += -EL
CROSS_CFLAGS += -march=mips32 -Os -g -fno-pic -mno-abicalls
CROSS_CFLAGS += -fno-strict-aliasing -fno-common -fomit-frame-pointer -G 0
CROSS_CFLAGS += -pipe -mlong-calls

OBJS_CORE= def.o dns.o inet_chksum.o init.o ip.o mem.o memp.o netif.o pbuf.o raw.o stats.o sys.o tcp.o tcp_in.o tcp_out.o timeouts.o udp.o

OBJS_IPV4=autoip.o dhcp.o etharp.o icmp.o igmp.o ip4.o ip4_addr.o ip4_frag.o
OBJS_IPV6= dhcp6.o ethip6.o icmp6.o inet6.o ip6_addr.o ip6.o ip6_frag.o mld6.o nd6.o

OBJS_DRV=ethernet.o

SRC=../src

all:	$(OBJS_CORE) $(OBJS_IPV4) $(OBJS_IPV6) $(OBJS_DRV)
	$(AR) rs liblwip.a $(OBJS_CORE) $(OBJS_IPV4) $(OBJS_IPV6) $(OBJS_DRV)

.c.o:
	$(CC) $(CROSS_CFLAGS) -Iinclude -I$(SRC)/include -c $<

clean:
	rm -rf *.o *.a
