#! /bin/bash
do_compile="true"
do_execute="true"
do_cpu_mode="false"
# do_use_cxx11="false"
do_use_cxx11="true"
do_pli_mode="false"
do_profile="false"
do_profile_all="false"
start="0"
stop="10000"
echo "$1"
if [ ! -n "$1" ]; then
  echo "Please input the source path!"
  exit 1
fi

ls $1 > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "Wrong source path!"
  exit 1
fi

if ! which pintcc > /dev/null 2>&1;then
  echo "No pint_sdk found, you will run with cpu mode. if you want to ran with Pint card, please install pint_sdk to compile!!!"
  do_cpu_mode="true"
fi
mkdir -p $1/log_file > /dev/null 2>&1
mkdir -p $1/out_file > /dev/null 2>&1
for arg in $*
do
  if [ $arg"x" = "cleanx" ];then
    rm -rf $1/*
    exit 0
  elif [ $arg"x" = "compile_onlyx" ];then
    do_execute="false"
    echo compile_only
  elif [ $arg"x" = "run_onlyx" ];then
    do_compile="false"
    echo run_only
  elif [ $arg"x" = "cpu_modex" ];then
    do_cpu_mode="true"
    echo cpu_mode
  elif [ $arg"x" = "use_cxx11x" ];then
    do_use_cxx11="true"
    echo use_cxx11
  elif [ $arg"x" = "pli_modex" ];then
    do_pli_mode="true"
    echo pli_mode
  elif [ $arg"x" = "profilex" ];then
    do_profile="true"
    echo do_profile
    continue
  elif [ $arg"x" = "profile_allx" ];then
    do_profile_all="true"
    do_profile="true"
    echo do_profile_all
    continue    
  elif [ ${arg%=*} = "start" ];then
    start=${arg##*=}
    echo "start_parallel_cnt=$start"
    continue
  elif [ ${arg%=*} = "stop" ];then
    stop=${arg##*=}
    echo "stop_parallel_cnt=$stop"
    continue
  elif [ $arg"x" = "-hx" ];then
    echo "USAGE: ./a.out|./your_exe arg"
    echo "arg: compile_only->only compile not run simulation"
    echo "     run_only->only run simulation not compile"
    echo "     clean->delete all the output files in *_pint_simu dir"
    exit 0
  else
    if [ -f $arg ];then
      tail_line=$(tail -1 $arg)
      dump_line="\$enddefinitions \$end"
      if [ "$tail_line" = "$dump_line" ];then
        cp $arg $1/out_file/ > /dev/null 2>&1
      else
        cp $1/out_file/$arg $arg > /dev/null 2>&1
      fi
    fi
  fi
  if [ $1"x" = $arg"x" ]; then
    continue
  fi
  if [ "cpu_modex" = $arg"x" ]; then
    continue
  fi
  if [ "run_onlyx" = $arg"x" ]; then
    continue
  fi  
  run_paremeter="$run_paremeter $arg"
  echo "run_paremeter: $run_paremeter"
done

pushd $1/out_file > /dev/null 2>&1
echo $$ > pid.txt

dev_num=0
dev_num=`lspci | grep 1e57 | wc -l`
echo "pint dev number: $dev_num"

function compile_percent()
{
  for (( ; ;)) do
    compile_file_num=`flock -ne pint_ivl_tool -c "read_file.sh 1"`
    if [ $? -eq 1 ]; then
      continue
    fi
    percent=`echo "scale=2; $compile_file_num / $total_file_num" | bc`
    percent=`echo "scale=0; $percent * 100" | bc`
    echo -ne "\rCompiling [${percent%.*}%]"
    break
  done
}

function collect_error
{
  for (( ; ;)) do
    flock -ne pint_ivl_tool -c "read_file.sh 2"
    if [ $? -eq 1 ]; then
      continue
    fi
    break
  done
}

function thread_run
{               
  for(( ; ;)) do
    name=`flock -ne pint_ivl_tool -c read_file.sh`
    if [ $? -eq 1 ]; then
      continue 
    else 
      if [ $name = "empty" ];then
        break 
      fi 
    fi
    md5val=`md5sum $name`
    file_name=${name##*/}

    md5_new=${md5val% *}
    md5_old=`cat $1/${file_name%.*}.md5 2>&1` > /dev/null 2>&1
    if [ "$md5_new" == "$md5_old" ]; then
      echo "$name same to old" > /dev/null 2>&1
      compile_percent
      continue 
    fi
    echo "Compile $name" > /dev/null 2>&1
    $compile_cmd$name >> $1/log_file/${file_name%.*}.log 2>&1
    if [ $? -eq 0 ]; then
      echo "$name compile done!" > /dev/null 2>&1
      compile_percent
      echo "${md5val% *}" > $1/${file_name%.*}.md5
    else
      echo " "
      echo "$name compile fail!"
      compile_percent
      rm $1/${file_name%.*}.md5 > /dev/null 2>&1
      collect_error
    fi
  done
}
if [ "$do_compile" = "true" ];then
  start_time=`echo "scale=7; $(date +%s%N)/1000000000" | bc`
  src_path=$1/src_file
  echo $src_path > /dev/null 2>&1
  include_dir="-I$PINT_INCLUDE_DIR -I$src_path -I/usr/local/include"
  if [ -f tmp.txt ];then
    rm -rf tmp.txt
  fi
  cpu_core_num=`cat /proc/cpuinfo | grep -cE "processor"`
  thread_file_list=`ls -S $src_path/thread*.c 2> /dev/null`
  lpm_file_list=`ls -S $src_path/lpm*.c 2> /dev/null`
  nbassign_file_list=`ls $src_path/nbassign_init*.c 2> /dev/null`
  totalmem_b=$(echo `cat /proc/meminfo | awk '{print $2}'` | awk '{print $1}')
  totalmem_g=`expr $totalmem_b / 1024 / 1024`
  mem_use=`expr $cpu_core_num \* 3`
  if [ $mem_use -gt $totalmem_g ];then
    thread_max=`expr $totalmem_g / 3`
  else
    thread_max=`expr $cpu_core_num - 1`
  fi
  filelist=(" $src_path/pint_thread_stub.c ${nbassign_file_list[@]} ${thread_file_list[@]} ${lpm_file_list[@]}")
  export total_file_num=1
  echo "0" > tmp1.txt
  echo "0" > tmp2.txt
  for file in ${filelist}   
  do
    total_file_num=`expr $total_file_num + 1`
    echo $file >> tmp.txt   
  done
  if [ "$do_cpu_mode" = "false" ];then
  if [ "$do_pli_mode" = "false" ];then
  compile_cmd="riscv32-unknown-elf-g++ -std=c++1z -falign-functions=64 -Wl,--gc-sections --param max-inline-insns-single=200 --param max-inline-insns-auto=40 -pipe -fpermissive -Wno-write-strings -nostdlib -nostartfiles -fno-builtin -fsigned-char -I$PINT/lib -I$PINT/include -march=rv32imf -mabi=ilp32f --pipe -O2 -fno-schedule-insns -fno-schedule-insns2 $include_dir -c "
  else
  compile_cmd="riscv32-unknown-elf-g++ -std=c++1z -DPINT_PLI_MODE -falign-functions=64 -Wl,--gc-sections --param max-inline-insns-single=200 --param max-inline-insns-auto=40 -pipe -fpermissive -Wno-write-strings -nostdlib -nostartfiles -fno-builtin -fsigned-char -I$PINT/lib -I$PINT/include -march=rv32imf -mabi=ilp32f --pipe -O2 -fno-schedule-insns -fno-schedule-insns2 $include_dir -c "
  fi
  else
  if [ "$do_use_cxx11" = "false" ];then
  # compile_cmd="g++ -g -std=c++1z -mcmodel=large -fpermissive -DCPU_MODE -I$PINT/include -O2 -fno-schedule-insns -fno-schedule-insns2 $include_dir -c "
  compile_cmd="g++ -std=c++1z -mcmodel=large -fpermissive -DCPU_MODE -I$PINT/include -O2 -fno-schedule-insns -fno-schedule-insns2 $include_dir -c "
  else
  # compile_cmd="g++ -g -std=c++11 -mcmodel=large -fpermissive -DCPU_MODE -DUSE_CXX11 -I$PINT/include -O2 -fno-schedule-insns -fno-schedule-insns2 $include_dir -c "
  compile_cmd="g++ -std=c++11 -mcmodel=large -fpermissive -DCPU_MODE -DUSE_CXX11 -I$PINT/include -O2 -fno-schedule-insns -fno-schedule-insns2 $include_dir -c "
  fi
  fi
  if [ "$do_profile_all" = "true" ];then
  compile_cmd="riscv32-unknown-elf-g++ -std=c++1z -DPROF_SUMMARY_ON -falign-functions=64 -Wl,--gc-sections --param max-inline-insns-single=200 --param max-inline-insns-auto=40 -pipe -fpermissive -Wno-write-strings -nostdlib -nostartfiles -fno-builtin -fsigned-char -I$PINT/lib -I$PINT/include -march=rv32imf -mabi=ilp32f --pipe -O2 -fno-schedule-insns -fno-schedule-insns2 $include_dir -c "
  fi
  rm -rf $1/log_file/*
  echo -ne "\rCompiling [0%]"
  export error_flag=0
  for((i=0;i<`expr $thread_max`;i++))
  do
  {
    thread_run $1
  } &
  done
  wait
  error_flag=`cat tmp2.txt`
  if [ $error_flag -ne 0 ];then
    compile_percent
    echo
    echo "There are some files compile failed, because some functions not support. exit!"
    exit 1
  fi
  pintcc_cmd=" "
  for thread_file in $thread_file_list
  do
    thread_file_name=${thread_file##*/}
    pintcc_cmd="$pintcc_cmd $1/out_file/${thread_file_name%.*}.o"
  done

  for lpm_file in $lpm_file_list
  do
    lpm_file_name=${lpm_file##*/}
    pintcc_cmd="$pintcc_cmd $1/out_file/${lpm_file_name%.*}.o"
  done

  for nbassign_file in $nbassign_file_list
  do
    nbassign_file_name=${nbassign_file##*/}
    pintcc_cmd="$pintcc_cmd $1/out_file/${nbassign_file_name%.*}.o"
  done

  date1=`date +%s.%N`

  #profile
  if [ "$do_profile" = "true" ];then
    riscv32-unknown-elf-ar rvs pint_thread.a ${pintcc_cmd} $1/out_file/pint_thread_stub.o
    if [ "$do_profile_all" = "true" ];then
      echo "pint_simu_prof $1/out_file/pint_thread.a $run_paremeter"
      pint_simu_prof $1/out_file/pint_thread.a $run_paremeter
    else    
      echo "pint_simu_prof $1/out_file/pint_thread.a $start $stop $run_paremeter"
      pint_simu_prof $1/out_file/pint_thread.a $start $stop $run_paremeter
    fi
    exit $?
  fi

  if [ "$do_cpu_mode" = "false" ];then
    if [ "$COMPILE_OPT" = "ON" ];then
      if [ "$do_pli_mode" = "false" ];then
        ld.lld -o pint.out -T $PINT/script/uapcc-elf.ld -nostdlib --whole-archive --gc-sections $PINT/lib/head2main.o $PINT/lib/pintdev.a /usr/local/lib/pintsimu.a ${pintcc_cmd} $1/out_file/pint_thread_stub.o
      else
        ld.lld -o pint.out -T $PINT/script/uapcc-elf.ld -nostdlib --whole-archive --gc-sections $PINT/lib/head2main.o $PINT/lib/pintdev.a /usr/local/lib/pintsimu_pli.a ${pintcc_cmd} $1/out_file/pint_thread_stub.o
      fi
      if [ $? -ne 0 ];then
        compile_percent
        echo
        echo "lld error: you can try export COMPILE_OPT=OFF then run again!"
        exit 1
      fi
    else
      if [ "$do_pli_mode" = "false" ];then
        riscv32-unknown-elf-ld -o pint.out -T $PINT/script/uapcc-elf.ld -nostdlib --whole-archive --gc-sections $PINT/lib/head2main.o $PINT/lib/pintdev.a /usr/local/lib/pintsimu.a ${pintcc_cmd} $1/out_file/pint_thread_stub.o
      else
        riscv32-unknown-elf-ld -o pint.out -T $PINT/script/uapcc-elf.ld -nostdlib --whole-archive --gc-sections $PINT/lib/head2main.o $PINT/lib/pintdev.a /usr/local/lib/pintsimu_pli.a ${pintcc_cmd} $1/out_file/pint_thread_stub.o
      fi
      if [ $? -ne 0 ];then
        compile_percent
        echo
        exit 1
      fi
    fi
  fi
  date2=`date +%s.%N`
  echo `echo "scale=7; $date2-$date1" | bc` > /dev/null 2>&1

  if [ "$do_cpu_mode" = "false" ];then
  pintcc -o pint.out -build_binary
  else
  # g++ -g -mcmodel=large -o verilog_cpu ${pintcc_cmd} $1/out_file/pint_thread_stub.o /usr/local/lib/pintsimu_cpu.a
  g++ -mcmodel=large -o verilog_cpu ${pintcc_cmd} $1/out_file/pint_thread_stub.o /usr/local/lib/pintsimu_cpu.a
  fi
  if [ $? -ne 0 ];then
    compile_percent
    echo
    exit 1
  fi
  date3=`date +%s.%N`
  echo `echo "scale=7; $date3-$date2" | bc` > /dev/null 2>&1

  if [ "$do_cpu_mode" = "false" ];then
  pint_generate_pin . pint
  fi
  if [ $? -ne 0 ];then
    compile_percent
    echo
    exit 1
  fi
  compile_percent
  echo
  date4=`date +%s.%N`
  echo `echo "scale=7; $date4-$date3" | bc` > /dev/null 2>&1


  end_time=`echo "scale=7; $(date +%s%N)/1000000000" | bc`
  total_time=`echo "scale=7; $end_time-$start_time" | bc`
  echo "compile time is ${total_time}s"
fi

if [ "$do_execute" = "true" ];then
  if [ "$do_cpu_mode" = "false" ];then
    if [ $dev_num -ne 0 ]; then
      pint_simu_stub $run_paremeter
    else
      echo "No pint device was found, please make sure your environment has a pint card!"
    fi
  else
    echo "verilog simu run in cpu mode: "
    echo "./verilog_cpu $run_paremeter"
    ./verilog_cpu $run_paremeter
  fi
fi
popd > /dev/null
