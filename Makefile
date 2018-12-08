#
# https://wiki.osdev.org/CFE_Bare_Bones
#

NEWLIBDIR=newlib-3.0.0.20180831
LWIPDIR=lwip-2.1.2
BARESSLDIR=bearssl-0.6

CROSS=mips

CROSS_LDFLAGS = -static
CROSS_LIBS = -L./$(NEWLIBDIR)/mips/newlib
CROSS_LIBS += -L../hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/
CROSS_LIBS += -Lmruby/build/broadcom/lib/
CROSS_LIBS += -L$(LWIPDIR)/broadcom/
CROSS_LIBS += -L./$(BARESSLDIR)/build/
CROSS_LIBS += -L./cfe/
CROSS_LIBS += -lmruby -llwip -lbearssl -lcfe -lc -lgcc

CROSS_CFLAGS = -I./$(NEWLIBDIR)/newlib/libc/include/
CROSS_CFLAGS += -I./mruby/include
CROSS_CFLAGS += -I./$(LWIPDIR)/src/include -I./$(LWIPDIR)/broadcom/include
CROSS_CFLAGS += -I$(BARESSLDIR)/inc
CROSS_CFLAGS +=  -march=mips32 -Os -g -fno-pic -mno-abicalls
CROSS_CFLAGS += -fno-strict-aliasing -fno-common -fomit-frame-pointer -G 0
CROSS_CFLAGS += -pipe -mlong-calls
CROSS_CFLAGS += -DUSE_INQUEUE=1

OBJS = start.o main.o syscalls.o
OBJS += bcm_timer.o bcm_ether.o bcm_gpio.o
OBJS += xprintf.o mt19937ar.o
OBJS += net.o bear.o
OBJS += ver.o

#RBSCRIPT = hoge.rb
#RBSCRIPT = test/udp.rb
RBSCRIPT = test/http.rb
#RBSCRIPT = test/simplehttp.rb

main.elf : $(OBJS) $(CFE_OBJS)
	./ver.sh
	$(CROSS)-cc $(CROSS_CFLAGS) -c ver.c
	$(CROSS)-ld $(CROSS_LDFLAGS) -T main.ld -o main.elf $(OBJS) $(CROSS_LIBS)

start.o : start.S
	$(CROSS)-as -o start.o start.S

main.o : main.c $(RBSCRIPT)
	./mruby/build/host/bin/mrbc -Bbytecode -ohoge.c $(RBSCRIPT)
	$(CROSS)-cc $(CROSS_CFLAGS) -o main.o -c main.c

.c.o:
	$(CROSS)-cc -O2 $(CROSS_CFLAGS) -c $<

syscalls.o : syscalls.c
bcm_ether.o : bcm_ether.c
bcm_gpio.o : bcm_gpio.c
bcm_timer.o : bcm_timer.c
xprintf.o : xprintf.c
mt19937ar.o : mt19937ar.c
net.o : net.c
bear.o : bear.c

image :
	$(CROSS)-objcopy -O binary main.elf main.bin
	gzip -f --best main.bin
	tools/asustrx -o main.trx main.bin.gz

clean:
	rm -rf main.elf $(OBJS) hoge.c
