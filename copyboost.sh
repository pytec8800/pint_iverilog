#!/bin/bash

## author: Yingchao
## extract boost header file we need

HeaderList=./graph_path.txt
DstRootDir=./boost_new/

for line in `cat ${HeaderList}`
do
    #echo "${DstRootDir}${line#*boost\/}"
    F="${DstRootDir}${line#*\/boost\/}"
    Dir=`dirname ${F}`
    mkdir -p ${Dir}
    cp ${line} ${F}
done
