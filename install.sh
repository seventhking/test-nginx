#!/bin/bash

pwd=`pwd`

cd ${pwd}/nginx-1.10.1
./configure --prefix=${pwd}/nginx --with-http_gunzip_module --add-module=${pwd}/my-module/*

processor_num=`cat /proc/cpuinfo |grep processor |wc -l`

make -j $processor_num && make install
