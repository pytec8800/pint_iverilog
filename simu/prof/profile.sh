export PINT_COMPILE_MODE=SINGLE
export PINT_MODE_PERF=ON
#/home/sqing/work/stub_ref/pint_iverilog/install/bin/iverilog -t pint /home/sqing/work/stub/pint_iverilog/pintapi/test/pic.v > generate.log
$1 > generate.log
cat generate.log
pint_simu_prof 16 $2 ./a.out_pint_simu/src_file/pint_thread_stub.c 0x00000000
#pint_simu_prof 16 /home/sqing/work/uaptool/simu/ ./a.out_pint_simu/src_file/pint_thread_stub.c 0x00000000
#pint_simu_prof 16 /home/sqing/work/stub/pint_iverilog/pintapi/ ./a.out_pint_simu/src_file/pint_thread_stub.c 0x00000000
unset PINT_COMPILE_MODE
unset PINT_MODE_PERF
#./profile.sh "/home/sqing/work/stub_ref/pint_iverilog/install/bin/iverilog -t pint -D MEM_SIZE=524288 -D MEM_SIZE_BIT=19 -I ../rtl/  ./tb_8dut.v" /home/sqing/work/uaptool/simu/

#pint_simu_prof 16 /home/sqing/work/uaptool/simu/ ./pint_thread_stub.c 0x58001fff 0 0 600  0x3c3e2 0x62
