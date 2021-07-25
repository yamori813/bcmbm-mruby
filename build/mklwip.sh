#!/bin/sh

LWIP=lwip-2.1.2

cd work

rm -rf ${LWIP}

unzip ${LWIP}.zip

START=`date '+%s'`

cp -r ../lwip ${LWIP}/broadcom

cd ${LWIP}/broadcom;make

END=`date '+%s'`

TIME=`expr ${END} - ${START}`

echo "${TIME} sec"
