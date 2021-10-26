#!/bin/sh

IP=$1
FILE=$2

while [ 1 ]
do
ping -t 1 -c 2 ${IP} >/dev/null 2>&1
if [ $? == 0 ]; then
echo " Find target"
echo "bin
put ${FILE}
quit
" | tftp ${IP}
exit
fi
echo -n "."
done
