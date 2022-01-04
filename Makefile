#
# bcmbm-mruby Makefile
#

NEWLIBDIR=newlib-3.0.0.20180831
LWIPDIR=lwip-2.1.2
BARESSLDIR=bearssl-0.6

CROSS=mips
CROSSBU=mips-unknown-freebsd13.0

CROSS_LDFLAGS = -static -EL
CROSS_LIBS = -Lbuild/work/$(NEWLIBDIR)/mips/el/newlib/
CROSS_LIBS += -L/usr/local/lib/gcc/mips/4.9.2/el/
CROSS_LIBS += -Lmruby/build/broadcom/lib/
CROSS_LIBS += -Lbuild/work/$(LWIPDIR)/mips4kel/
CROSS_LIBS += -Lbuild/work/$(BARESSLDIR)/build/
CROSS_LIBS += -L./cfe/
CROSS_LIBS += -lmruby -llwip -lbearssl -lcfe -lc -lgcc

CROSS_CFLAGS = -Ibuild/work/$(NEWLIBDIR)/newlib/libc/include/
CROSS_CFLAGS += -I./mruby/include
CROSS_CFLAGS += -Ibuild/work/$(LWIPDIR)/src/include
CROSS_CFLAGS += -Ibuild/work/$(LWIPDIR)/mips4kel/include
CROSS_CFLAGS += -Ibuild/work/$(BARESSLDIR)/inc
CROSS_CFLAGS += -march=mips32 -EL -G 0
CROSS_CFLAGS += -Os -g -fno-pic -mno-abicalls
CROSS_CFLAGS += -fno-strict-aliasing -fno-common -fomit-frame-pointer
CROSS_CFLAGS += -pipe -mlong-calls

OBJS = start.o main.o syscalls.o
OBJS += bcm_timer.o bcm_ether.o bcm_gpio.o
OBJS += xprintf.o mt19937ar.o
OBJS += net.o bear.o
OBJS += i2c.o

RBSCRIPT = samples/hello.rb

main.bin.gz : $(OBJS) cfe/libcfe.a
	./ver.sh
	$(CROSS)-gcc $(CROSS_CFLAGS) -c ver.c
	$(CROSSBU)-ld $(CROSS_LDFLAGS) -T main.ld -o main.elf $(OBJS) ver.o $(CROSS_LIBS)
	$(CROSSBU)-objcopy -O binary main.elf main.bin
	gzip -f --best main.bin

start.o : start.S
	$(CROSSBU)-as -EL -o start.o start.S

.c.o:
	$(CROSS)-gcc -O2 $(CROSS_CFLAGS) -c $<

image :
	./mruby/build/host/mrbc/bin/mrbc -ohoge.mrb $(RBSCRIPT)
	tools/asustrx -o main.trx main.bin.gz hoge.mrb

clean:
	rm -rf main.elf $(OBJS) hoge.c ver.c
