NEWLIBDIR=newlib-3.0.0.20180831

CROSS_CFLAGS = -I../$(NEWLIBDIR)/newlib/libc/include/
CROSS_CFLAGS += -Os -g -fno-pic -mno-abicalls
CROSS_CFLAGS += -fno-strict-aliasing -fno-common -fomit-frame-pointer -G 0
CROSS_CFLAGS += -pipe -mlong-calls
CROSS_CFLAGS += -DBCM47XX -DENDIAN_LITTLE=1 -DIPOLL=1 -march=mips32 -DCPUCFG_REGS32=1 -DCFG_INTERRUPTS=1 -DCFG_RUNFROMKSEG0=1 -DCFG_RELOC=1 -DCFE_API_ALL -DENDIAN_BIG=0

OBJS = exception.o bcmcore_l1cache.o exchandler.o bcmcore_ircpoll.o lib_try.o lib_queue.o lib_setjmp.o dev_sb_mac.o lib_physio.o cfe_timer.o bcmcore_irq.o sb_utils.o net_ether.o apientry.o bcmcore_cpuinit.o
OBJS += cfe_api.o bcmnvram.o
OBJS += etc_robo.o

libcfe.a : $(OBJS)
	mips-ar rs libcfe.a $(OBJS)

.S.o:
	mips-cc -O2 $(CROSS_CFLAGS) -c $<

.c.o:
	mips-cc -O2 $(CROSS_CFLAGS) -c $<

exception.o : exception.S
bcmcore_l1cache.o : bcmcore_l1cache.S
bcmcore_cpuinit.o : bcmcore_cpuinit.S
apientry.o : apientry.S
lib_setjmp.o : lib_setjmp.S
lib_physio.o : lib_physio.S

exchandler.o : exchandler.c
bcmcore_irq.o : bcmcore_irq.c
bcmcore_ircpoll.o : bcmcore_ircpoll.S
lib_try.o : lib_try.c
lib_queue.o : lib_queue.c
dev_sb_mac.o : dev_sb_mac.c
cfe_timer.o : cfe_timer.c
sb_utils.o : sb_utils.c
net_ether.o : net_ether.c
cfe_api.o : cfe_api.c
bcmnvram.o : bcmnvram.c
etc_robo.o : etc_robo.c

clean :
	rm -rf $(OBJS)
