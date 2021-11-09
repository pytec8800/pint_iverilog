#./build/pint_simu_prof 1 ../ ../test/pint_thread_stub.c 0x5 #0x1FC3 #0x07
#./build/pint_simu_prof 16 ../ ../test/pint_thread_stub.c 0x5 #0x1FC3 #0x07
#../../install/bin/iverilog -t pint -I ../test/sharecache/rtl/  ../test/sharecache/ben/tb_8dut.v

#../../../install/bin/iverilog -t pint -D MEM_SIZE=524288 -D MEM_SIZE_BIT=19 \
#-I ../../test/sharecache/rtl/  ../../test/sharecache/ben7/tb_8dut.v

../../../install/bin/iverilog -t pint -D MEM_SIZE=16384 -D MEM_SIZE_BIT=14 \
-I sharecache/rtl/  sharecache/ben7/tb_8dut.v

#../../../install/bin/iverilog -t pint -I ../../test/sharecache/rtl/  ../../test/sharecache/ben/tb_1dut.v

#../../../install/bin/iverilog -t pint ../../test/pic.v
#../build/pint_simu_prof 16 ../../ ./pint_thread_stub.c 0x40000403 1 0x0 20 100
# ../build/pint_simu_prof 16 ../../ ./pint_thread_stub.c 0x20000000 0 0x0 20 100
#../build/pint_simu_prof 16 ../../ ./pint_thread_stub.c 0xd8001FFF 0 0 80  0x3c3e2 0x62
# for enable parallel_cnt recording and analyze timeline.json
../build/pint_simu_prof 16 ../../ ./pint_thread_stub.c 0x40000003 0 0 30000 0x3c3e2 0x62 0
#../build/pint_simu_prof 16 ../../ ./pint_thread_stub.c 0xa0000000 0 20 40 0x3c3d2 0x62 0 > record.txt
#../build/pint_simu_prof 16 ../../ ./pint_thread_stub.c 0xc0000000 0 0 99999999  0x3c3e2 0x62
#grep mcore-measure ./record.txt > mcore_record_one_thread.csv


