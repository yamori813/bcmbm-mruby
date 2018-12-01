#
# https://wiki.osdev.org/CFE_Bare_Bones
#

NEWLIBDIR=newlib-3.0.0.20180831

CROSS=mips

CROSS_LDFLAGS = -static
CROSS_LIBS = -L./$(NEWLIBDIR)/mips/newlib
CROSS_LIBS += -L../hndtools-mipsel-linux-uclibc-4.2.3/lib/gcc/mipsel-linux-uclibc/4.2.3/
CROSS_LIBS += -Lmruby/build/broadcom/lib/
CROSS_LIBS += -lmruby -lc -lgcc

CROSS_CFLAGS = -I./$(NEWLIBDIR)/newlib/libc/include/
CROSS_CFLAGS += -I./mruby/include
CROSS_CFLAGS += -Os -g -fno-pic -mno-abicalls
CROSS_CFLAGS += -fno-strict-aliasing -fno-common -fomit-frame-pointer -G 0
CROSS_CFLAGS += -pipe -mlong-calls

OBJS = start.o main.o syscalls.o

main.elf : $(OBJS)
	$(CROSS)-ld $(CROSS_LDFLAGS) -T main.ld -o main.elf $(OBJS) $(CROSS_LIBS)

start.o : start.S
	$(CROSS)-as -o start.o start.S

main.o : main.c
	./mruby/build/host/bin/mrbc -Bbytecode hoge.rb
	$(CROSS)-cc $(CROSS_CFLAGS) -o main.o -c main.c

syscalls.o : syscalls.c
	$(CROSS)-cc $(CROSS_CFLAGS) -o syscalls.o -c syscalls.c

clean:
	rm -rf main.elf start.o main.o hoge.c
