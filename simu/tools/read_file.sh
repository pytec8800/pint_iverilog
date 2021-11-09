#!/bin/bash
function read_it
{
test -s tmp.txt
file_empty=$?
if [ $file_empty -ne 0 ]; then
  echo "empty"
  return
fi
head -n +1 tmp.txt
sed -i '1d' tmp.txt
}

function change_it
{
  num=`cat tmp1.txt`
  num=`expr $num + 1`
  echo $num
  echo $num > tmp1.txt
}

function collect_it
{
  num=`cat tmp2.txt`
  num=`expr $num + 1`
  echo $num > tmp2.txt
}

if [ $# -eq 0 ];then
  read_it
elif [ $1 -eq 1 ];then
  change_it
elif [ $1 -eq 2 ];then
  collect_it
fi
