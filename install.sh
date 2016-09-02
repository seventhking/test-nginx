#!/bin/bash

pwd=`pwd`
if [ ! -d "nginx" ]; then
    cd ${pwd}/dependences
    tar -xvzf nginx-1.8.1.tar.gz
fi

cd ${pwd}/dependences/nginx-1.8.1
./configure --prefix=${pwd}/nginx --with-http_gunzip_module

processor_num=`cat /proc/cpuinfo |grep processor |wc -l`

make -j $processor_num && make install
