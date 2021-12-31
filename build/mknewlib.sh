#!/bin/sh

OS=`uname -s`

NEWLIB=newlib-3.0.0.20180831

cd work

rm -rf ${NEWLIB}

tar -zxf ${NEWLIB}.tar.gz

sed -i -e '/^	FPR_LAYOUT$/d' ${NEWLIB}/newlib/libc/machine/mips/setjmp.S

START=`date '+%s'`

if [ "$OS" = 'Linux' ]; then
cd ${NEWLIB};./configure --target=mips;make
else
VER=`uname -r | sed 's/-.*//'`
ARCH=`uname -p`
MIPSBU=mips-unknown-freebsd${VER}
cd ${NEWLIB};./configure --host=${ARCH} --target=mips CC_FOR_BUILD=cc CFLAGS_FOR_TARGET="-march=mips32 -G 0 -fno-pic -mno-abicalls" ac_cv_path_AR_FOR_TARGET=${MIPSBU}-ar ac_cv_path_RANLIB_FOR_TARGET=${MIPSBU}-ranlib;gmake -j8
fi

END=`date '+%s'`

TIME=`expr ${END} - ${START}`

echo "${TIME} sec"
