#!/bin/sh

LWIP=lwip-2.1.2

cd work

rm -rf ${LWIP}

unzip ${LWIP}.zip

START=`date '+%s'`

cp -r ../lwip ${LWIP}/mips4kel

cd ${LWIP}/mips4kel;make AR=mips-unknown-freebsd13.0-ar

END=`date '+%s'`

TIME=`expr ${END} - ${START}`

echo "${TIME} sec"
