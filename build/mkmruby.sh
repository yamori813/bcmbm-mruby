#!/bin/sh

CDIR=`pwd`

START=`date '+%s'`

cd ../mruby

rm -rf build

rake MRUBY_CONFIG=${CDIR}/mruby/build_config.rb

END=`date '+%s'`

TIME=`expr ${END} - ${START}`

echo "${TIME} sec"
