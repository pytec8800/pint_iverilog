#!/bin/bash

if [ ! -d './install/' ];then
mkdir ./install
else
echo 'install dir existed.'
fi

sh autoconf.sh
./configure --prefix=${PWD}/install #CFLAGS="-g -O0" CXXFLAGS="-g -O0"

make -j8
make install

