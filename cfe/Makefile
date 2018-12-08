NEWLIBDIR=newlib-3.0.0.20180831

CROSS_CFLAGS = -I../$(NEWLIBDIR)/newlib/libc/include/
CROSS_CFLAGS += -Os -g -fno-pic -mno-abicalls
CROSS_CFLAGS += -fno-strict-aliasing -fno-common -fomit-frame-pointer -G 0
CROSS_CFLAGS += -pipe -mlong-calls
CROSS_CFLAGS += -DBCM47XX -DENDIAN_LITTLE=1 -DIPOLL=1 -march=mips32 -DCPUCFG_REGS32=1 -DCFG_INTERRUPTS=1 -DCFG_RUNFROMKSEG0=1 -DCFG_RELOC=1

OBJS = exception.o bcmcore_l1cache.o exchandler.o bcmcore_ircpoll.o lib_try.o lib_queue.o lib_setjmp.o dev_sb_mac.o lib_physio.o cfe_timer.o bcmcore_irq.o sb_utils.o net_ether.o apientry.o bcmcore_cpuinit.o

libcfe.a : $(OBJS)
	mips-ar rs libcfe.a $(OBJS)

exception.o : exception.S
	mips-cc $(CROSS_CFLAGS) -c exception.S

bcmcore_l1cache.o : bcmcore_l1cache.S
	mips-cc $(CROSS_CFLAGS) -c bcmcore_l1cache.S

bcmcore_cpuinit.o : bcmcore_cpuinit.S
	mips-cc $(CROSS_CFLAGS) -c bcmcore_cpuinit.S

apientry.o : apientry.S
	mips-cc $(CROSS_CFLAGS) -c apientry.S

exchandler.o : exchandler.c
	mips-cc $(CROSS_CFLAGS) -c exchandler.c

bcmcore_irq.o : bcmcore_irq.c
	mips-cc $(CROSS_CFLAGS) -c bcmcore_irq.c

bcmcore_ircpoll.o : bcmcore_ircpoll.S
	mips-cc $(CROSS_CFLAGS) -c bcmcore_ircpoll.S

lib_try.o : lib_try.c
	mips-cc $(CROSS_CFLAGS) -c lib_try.c

lib_queue.o : lib_queue.c
	mips-cc $(CROSS_CFLAGS) -c lib_queue.c

lib_setjmp.o : lib_setjmp.S
	mips-cc $(CROSS_CFLAGS) -c lib_setjmp.S

dev_sb_mac.o : dev_sb_mac.c
	mips-cc $(CROSS_CFLAGS) -c dev_sb_mac.c

lib_physio.o : lib_physio.S
	mips-cc $(CROSS_CFLAGS) -c lib_physio.S

cfe_timer.o : cfe_timer.c
	mips-cc $(CROSS_CFLAGS) -c cfe_timer.c

sb_utils.o : sb_utils.c
	mips-cc $(CROSS_CFLAGS) -c sb_utils.c

net_ether.o : net_ether.c
	mips-cc $(CROSS_CFLAGS) -c net_ether.c

clean :
	rm -rf $(OBJS)
