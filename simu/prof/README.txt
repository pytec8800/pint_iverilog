# pint_simu_prof
============================= 
pint_simu_prof is a profile tool for pint_simu_stub. It will generate simu_timeline.json and simu_summary.csv. simu_timeline.json can be loaded by chrome://tracing which illustrate the multicore thread process timeline. simu_summary.csv count each thread or function's total cycles, calltimes,average time and cache_cnt.

1. how to build
cd PINT_IVERILOG_PATH/pintapi/prof
./build.sh

2. how to run pint_simu_prof
For example, we want profile the sharedcache 8dut longtime case. First generate code:
  ../../install/bin/iverilog -t pint -D MEM_SIZE=524288 -D MEM_SIZE_BIT=19 \
  -I ../test/sharecache/rtl/  ../test/sharecache/ben7/tb_8dut.v
Then run pint_simu_prof:
  ./build/pint_simu_prof 16 ../ ./pint_thread_stub.c 0x58001fff 0 0 600  0x3c3e2 0x62
The above command means profiling pint_simu_stub from parallel_cnt:0 to parallel_cn:600. Enable each kind of function profiling. Enable dcache miss count and dcache2sharedcache read ops count. 

===========================================
## use case
use case 1: Profile from parallel_cnt:0 to parallel_cn:600. Enable each kind of function profiling. Enable dcache miss count and dcache2sharedcache read ops count. The output is simu_timeline.json and simu_summary.csv.
./build/pint_simu_prof 16 ../ ./pint_thread_stub.c 0x58001fff 0 0 600  0x3c3e2 0x62 0

use case 2: Enable mcore measure(print cycle/parallelcnt/threadnum of each parallel mode). The output is mcore_record.csv
./build/pint_simu_prof 16 ../ ./pint_thread_stub.c 0xa0000000 0 0 0  0x0 0x0 0 > record.txt
grep mcore-measure ./record.txt > mcore_record.csv

use case 3: The profile data is too big if you want to profile a longtime case. Enable large_profile_data(eg. 8dut longtime 14bit). It will only generate thread summary csv file, no timeline json file. This mode will run all the parallel_cnt period.
./build/pint_simu_prof 16 ../ ./pint_thread_stub.c 0x18001fff 0 0 0 0x3c3e2 0x62 1

use case 4: # Enable parallel_cnt recording and analyze timeline.json. function_called_once.py will find out the thread only called once in a parallel mode. calculate_idle_time.py will calculate the ncore occupancy.
./build/pint_simu_prof 16 ../ ./pint_thread_stub.c 0x40000003 0 0 30000 0x3c3e2 0x62 0
python function_called_once.py
python calculate_idle_time.py

=========================================
## pint_simu_prof args description.
  arg1: ncore num to perf(default=16). should <=16 && >=1
  arg2: pintapi root(default=../)
  arg3: pint_thread_stub.c file path(default="./pint_thread_stub.c")
  arg4: profile mask. 32bit unsigned int
    prof_mask bit description: use mask[12:0] to choose which kinds of function is profiled.
      mask[12:0]:
          execute_active/NB/Mcore fuction:      ->  bit0
          thread/lpm/moveevent:   ->  bit1
          small fuction in pint_net.c: -> bit2
          pint_move_event_thread_list: -> bit3
          pint_[enqueue|dequeue]_thread: -> bit4
          pint_wait_on_event: -> bit5
          pint_enqueue_flag_set/clear: ->bit6
          pint_add_list_to_active/inactive: ->bit7
          pint_enqueue_nb_thread/dequeue: ->bit8
          pint_process_event_list: ->bit9
          pint_nb_assign/array: ->bit10
          pint_nb_array_process: ->bit11
          edge/edge_int/edge_int_value: ->bit12
      mask[31:27]
        bit27: enable ncore dcache counter-measure
        bit28: enable ncore dcache2sharedcache counter-measure
        bit29: use all 256 ncore to run
        bit30: enable mcore parallel range. if set, ncore-measure is controled by
          range-begin/range-end
        bit31: enable mcore measure(print cycle/parallelcnt/threadnum of each parallel mode)
  arg5: 0 -> build from source(default), 1 -> build from binary
  arg6: parallel_begin
  arg7: parallel_end
  arg8: dcache counter config in Hex format. Dcac_Perf_rqst_cfg which define how performance 
        counter (12'h3b) collect different core to D-cache transactions. 
        (default = 0x3c3e2, which means collect dcache ro range read word miss)
  arg9: scache counter config. Dcac_Scac_Perf_cfg which define how the performance counter 
        Dcac_Scac_perf_cnt count the transactions from D-cache to Shared Cache. 
        (default = 0x62, which means collect dcache to sharedcache all the read ops.)
  arg10(optional,default = false): Enable large_profile_data(eg. 8dut longtime 14bit). It will 
  only generate thread summary csv file, no timeline json file. This mode will run all the 
  parallel_cnt period.

