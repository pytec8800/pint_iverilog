#../../install/bin/iverilog -t pint -D MEM_SIZE=524288 -D MEM_SIZE_BIT=19 \
#-I ../test/sharecache/rtl/  ../test/sharecache/ben7/tb_8dut.v

#../../install/bin/iverilog -t pint -D MEM_SIZE=16384 -D MEM_SIZE_BIT=14 \
#-I ../test/sharecache/rtl/  ../test/sharecache/ben7/tb_8dut.v

../../install/bin/iverilog -t pint -I ../test/sharecache/rtl/  ../test/sharecache/ben/tb_1dut.v

# use case 1: Profile from parallel_cnt:0 to parallel_cn:600. Enable each kind of function profiling. Enable dcache miss count and dcache2sharedcache read ops count. The output is simu_timeline.json and simu_summary.csv.
#./build/pint_simu_prof 16 ../ ./pint_thread_stub.c 0x58001fff 0 0 600  0x3c3e2 0x62 0

# use case 2: Enable mcore measure(print cycle/parallelcnt/threadnum of each parallel mode). The output is mcore_record.csv
#./build/pint_simu_prof 16 ../ ./pint_thread_stub.c 0xa0000000 0 0 0  0x0 0x0 0 > record.txt
#grep mcore-measure ./record.txt > mcore_record.csv

# use case 3: The profile data is too big if you want to profile a longtime case. Enable large_profile_data(eg. 8dut longtime 14bit). It will only generate thread summary csv file, no timeline json file. This mode will run all the parallel_cnt period.
#./build/pint_simu_prof 16 ../ ./pint_thread_stub.c 0x18001fff 0 0 0 0x3c3e2 0x62 1

# use case 4: # Enable parallel_cnt recording and analyze timeline.json. function_called_once.py will find out the thread only called once in a parallel mode. calculate_idle_time.py will calculate the ncore occupancy.
./build/pint_simu_prof 16 ../ ./pint_thread_stub.c 0x40000003 0 0 20000 0x3c3e2 0x62 0
python function_called_once.py
python calculate_idle_time.py
