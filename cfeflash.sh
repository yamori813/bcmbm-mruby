#!/bin/sh

if [ $# -eq 2 ]; then

IP=$1
FILE=$2

OS=`uname`

if [ ${OS} = "FreeBSD" ]; then
OPT="-t 1 -c2"
else
OPT="-w 1 -c2"
fi

while [ 1 ]
do
ping ${OPT} ${IP} >/dev/null 2>&1
if [ $? -eq 0 ]; then
echo " Find target"
echo "bin
put ${FILE}
quit
" | tftp ${IP}
exit
fi
echo -n "."
done
else
echo "cfeflash.sh <ip address> <trx file>"
fi
