cmake_minimum_required(VERSION 2.6)
PROJECT(simu)

set(SIMU_PINT_MODE OFF)
set(RISCV /usr/local/pint/riscv)

if(EXISTS "/usr/local/pint/bin/pintcc")
    message("find pint card in your env, will install pint mode and cpu mode!")
    set(SIMU_PINT_MODE ON)
else()
    message("can not find pint card in your env, only install cpu mode!")
endif()

# option(SIMU_USE_CXX11 "simu use c++11" OFF)
option(SIMU_USE_CXX11 "simu use c++11" ON)

option(SIMU_RECORD "install with record for pint mode" OFF)

if(SIMU_PINT_MODE)
    message("install pint_simu_stub and pint_simu_prof!")
    add_subdirectory(tools)
    add_subdirectory(prof)
endif()

if(SIMU_PINT_MODE)
  set(INCLUDE_OPTION -I/usr/local/pint/include -I/usr/local/pint/include/uap_API -I${CMAKE_HOME_DIRECTORY}/inc/)

  # pint pls version lib
  set(COMPILE_OPTION -fpermissive -Wno-write-strings -fsigned-char -std=c++1z -falign-functions=64 -Wl,--gc-sections --param max-inline-insns-single=200 --param max-inline-insns-auto=40)
  add_custom_command( OUTPUT pintsimu.a
                      WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
                      COMMAND ${RISCV}/bin/riscv32-unknown-elf-g++ -nostartfiles -nostdlib ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/main.o ${INCLUDE_OPTION} -march=rv32imf -mabi=ilp32f -c -O2 ${CMAKE_HOME_DIRECTORY}/src/main.c -fno-builtin
                      COMMAND ${RISCV}/bin/riscv32-unknown-elf-g++ -nostartfiles -nostdlib ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/pint_simu.o ${INCLUDE_OPTION} -march=rv32imf -mabi=ilp32f -c -O2 ${CMAKE_HOME_DIRECTORY}/src/pint_simu.c -fno-builtin
                      COMMAND ${RISCV}/bin/riscv32-unknown-elf-g++ -nostartfiles -nostdlib ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/pint_net.o ${INCLUDE_OPTION} -march=rv32imf -mabi=ilp32f -c -O2 ${CMAKE_HOME_DIRECTORY}/src/pint_net.c -fno-builtin
                      COMMAND ${RISCV}/bin/riscv32-unknown-elf-g++ -nostartfiles -nostdlib ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/pint_vpi.o ${INCLUDE_OPTION} -march=rv32imf -mabi=ilp32f -c -O2 ${CMAKE_HOME_DIRECTORY}/src/pint_vpi.c -fno-builtin
                      COMMAND ${RISCV}/bin/riscv32-unknown-elf-g++ -nostartfiles -nostdlib ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/perf.o ${INCLUDE_OPTION} -march=rv32imf -mabi=ilp32f -c -O2 ${CMAKE_HOME_DIRECTORY}/src/perf.c -fno-builtin
                      COMMAND ar rcs -o ${CMAKE_HOME_DIRECTORY}/build/pintsimu.a ${CMAKE_HOME_DIRECTORY}/build/main.o ${CMAKE_HOME_DIRECTORY}/build/pint_simu.o ${CMAKE_HOME_DIRECTORY}/build/pint_net.o ${CMAKE_HOME_DIRECTORY}/build/pint_vpi.o ${CMAKE_HOME_DIRECTORY}/build/perf.o

                      DEPENDS ${CMAKE_HOME_DIRECTORY}/src/*.c ${CMAKE_HOME_DIRECTORY}/inc/*.h ${CMAKE_HOME_DIRECTORY}/common/*.h /usr/local/pint/include/*.h /usr/local/pint/include/uap_API/*.h ${CMAKE_HOME_DIRECTORY}/inc/*.inc
                      COMMENT "pint simu library compile"
                    )
  add_custom_target( pintsimu ALL
                      DEPENDS pintsimu.a)

  # pint pli version lib
  set(COMPILE_OPTION -fpermissive -DPINT_PLI_MODE -Wno-write-strings -fsigned-char -std=c++1z -falign-functions=64 -Wl,--gc-sections --param max-inline-insns-single=200 --param max-inline-insns-auto=40)
  add_custom_command( OUTPUT pintsimu_pli.a
                      WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
                      COMMAND ${RISCV}/bin/riscv32-unknown-elf-g++ -nostartfiles -nostdlib ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/main.o ${INCLUDE_OPTION} -march=rv32imf -mabi=ilp32f -c -O2 ${CMAKE_HOME_DIRECTORY}/src/main.c -fno-builtin
                      COMMAND ${RISCV}/bin/riscv32-unknown-elf-g++ -nostartfiles -nostdlib ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/pint_simu.o ${INCLUDE_OPTION} -march=rv32imf -mabi=ilp32f -c -O2 ${CMAKE_HOME_DIRECTORY}/src/pint_simu.c -fno-builtin
                      COMMAND ${RISCV}/bin/riscv32-unknown-elf-g++ -nostartfiles -nostdlib ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/pint_net.o ${INCLUDE_OPTION} -march=rv32imf -mabi=ilp32f -c -O2 ${CMAKE_HOME_DIRECTORY}/src/pint_net.c -fno-builtin
                      COMMAND ${RISCV}/bin/riscv32-unknown-elf-g++ -nostartfiles -nostdlib ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/pint_vpi.o ${INCLUDE_OPTION} -march=rv32imf -mabi=ilp32f -c -O2 ${CMAKE_HOME_DIRECTORY}/src/pint_vpi.c -fno-builtin
                      COMMAND ${RISCV}/bin/riscv32-unknown-elf-g++ -nostartfiles -nostdlib ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/perf.o ${INCLUDE_OPTION} -march=rv32imf -mabi=ilp32f -c -O2 ${CMAKE_HOME_DIRECTORY}/src/perf.c -fno-builtin
                      COMMAND ar rcs -o ${CMAKE_HOME_DIRECTORY}/build/pintsimu_pli.a ${CMAKE_HOME_DIRECTORY}/build/main.o ${CMAKE_HOME_DIRECTORY}/build/pint_simu.o ${CMAKE_HOME_DIRECTORY}/build/pint_net.o ${CMAKE_HOME_DIRECTORY}/build/pint_vpi.o ${CMAKE_HOME_DIRECTORY}/build/perf.o

                      DEPENDS ${CMAKE_HOME_DIRECTORY}/src/*.c ${CMAKE_HOME_DIRECTORY}/inc/*.h ${CMAKE_HOME_DIRECTORY}/common/*.h /usr/local/pint/include/*.h /usr/local/pint/include/uap_API/*.h ${CMAKE_HOME_DIRECTORY}/inc/*.inc
                      COMMENT "pint simu pli library compile"
                    )
  add_custom_target( pintsimu_pli ALL
                      DEPENDS pintsimu_pli.a)

endif()

if(NOT SIMU_USE_CXX11)
set(COMPILE_OPTION -g -std=c++1z -mcmodel=large -fpermissive -DCPU_MODE)
else()
set(COMPILE_OPTION -g -std=c++11 -mcmodel=large -fpermissive -DCPU_MODE -DUSE_CXX11)
endif()
set(INCLUDE_OPTION -I${CMAKE_HOME_DIRECTORY}/inc)

add_custom_command( OUTPUT pintsimu_cpu.a
                    WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
                    COMMAND g++ ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/main.o ${INCLUDE_OPTION} -c -O2 ${CMAKE_HOME_DIRECTORY}/src/main.c
                    COMMAND g++ ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/pint_simu.o ${INCLUDE_OPTION} -c -O2 ${CMAKE_HOME_DIRECTORY}/src/pint_simu.c
                    COMMAND g++ ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/pint_net.o ${INCLUDE_OPTION} -c -O2 ${CMAKE_HOME_DIRECTORY}/src/pint_net.c
                    COMMAND g++ ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/pint_vpi.o ${INCLUDE_OPTION} -c -O2 ${CMAKE_HOME_DIRECTORY}/src/pint_vpi.c
                    COMMAND g++ ${COMPILE_OPTION} -o ${CMAKE_HOME_DIRECTORY}/build/cpu_adapt.o ${INCLUDE_OPTION} -c -O2 ${CMAKE_HOME_DIRECTORY}/cpu/src/cpu_adapt.c
                    COMMAND ar rcs -o ${CMAKE_HOME_DIRECTORY}/build/pintsimu_cpu.a ${CMAKE_HOME_DIRECTORY}/build/main.o ${CMAKE_HOME_DIRECTORY}/build/pint_simu.o ${CMAKE_HOME_DIRECTORY}/build/pint_net.o ${CMAKE_HOME_DIRECTORY}/build/pint_vpi.o ${CMAKE_HOME_DIRECTORY}/build/cpu_adapt.o
                    
                    DEPENDS ${CMAKE_HOME_DIRECTORY}/src/*.c ${CMAKE_HOME_DIRECTORY}/cpu/src/*.c ${CMAKE_HOME_DIRECTORY}/inc/*.h ${CMAKE_HOME_DIRECTORY}/inc/*.inc
                    COMMENT "simu cpu library compile"
                  )

add_custom_target( pintsimu_cpu ALL 
                   DEPENDS pintsimu_cpu.a)

set(INSTALL_DIR /usr/local)
set(INCLUDE_INSTALL_DIR_SYS ${INSTALL_DIR}/include)
set(LIB_INSTALL_DIR_SYS ${INSTALL_DIR}/lib)
set(BINARY_INSTALL_DIR_SYS ${INSTALL_DIR}/bin)
if(SIMU_PINT_MODE)
    INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/build/pintsimu.a DESTINATION ${LIB_INSTALL_DIR_SYS}
                PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ)
    INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/build/pintsimu_pli.a DESTINATION ${LIB_INSTALL_DIR_SYS}
                PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ)
    INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/build/tools/pint_simu_stub DESTINATION ${BINARY_INSTALL_DIR_SYS}
                PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ)
    INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/build/prof/pint_simu_prof DESTINATION ${BINARY_INSTALL_DIR_SYS}
                PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ)
endif()

INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/build/pintsimu_cpu.a DESTINATION ${LIB_INSTALL_DIR_SYS}
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/pintsimu.h DESTINATION ${INCLUDE_INSTALL_DIR_SYS}
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/inc/pint_net.h DESTINATION ${INCLUDE_INSTALL_DIR_SYS}/pint_simu/inc
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/inc/pint_code_macro.h DESTINATION ${INCLUDE_INSTALL_DIR_SYS}/pint_simu/inc
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/inc/pint_net.inc DESTINATION ${INCLUDE_INSTALL_DIR_SYS}/pint_simu/inc
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/inc/pint_simu.h DESTINATION ${INCLUDE_INSTALL_DIR_SYS}/pint_simu/inc
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/inc/pint_vpi.h DESTINATION ${INCLUDE_INSTALL_DIR_SYS}/pint_simu/inc
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/inc/cpu_adapt.h DESTINATION ${INCLUDE_INSTALL_DIR_SYS}/pint_simu/inc
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/common/debug_helper.h DESTINATION ${INCLUDE_INSTALL_DIR_SYS}/pint_simu/common
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/common/perf_def.h DESTINATION ${INCLUDE_INSTALL_DIR_SYS}/pint_simu/common
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/common/perf.h DESTINATION ${INCLUDE_INSTALL_DIR_SYS}/pint_simu/common
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/common/pint_perf_counter.h DESTINATION ${INCLUDE_INSTALL_DIR_SYS}/pint_simu/common
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_READ)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/tools/pint_ivl_tool.sh DESTINATION ${BINARY_INSTALL_DIR_SYS}
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ RENAME pint_ivl_tool)
INSTALL(FILES ${CMAKE_HOME_DIRECTORY}/tools/read_file.sh DESTINATION ${BINARY_INSTALL_DIR_SYS}
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ WORLD_EXECUTE WORLD_READ)



