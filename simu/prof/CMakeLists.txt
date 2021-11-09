cmake_minimum_required(VERSION 2.8)
project(pint_simu_prof)

#set(RISCV $ENV{PINT}/riscv)
#set(UAPTOOL $ENV{PINT})

set(CMAKE_CXX_STANDARD 11)
set(PINT_LIB_INSTALL_DIR /usr/local/pint/lib)
set(PINT_INC_INSTALL_DIR /usr/local/pint/include ../common/)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -std=c++11 -O0 -g")

include_directories(${PINT_INC_INSTALL_DIR})
link_directories(${PINT_LIB_INSTALL_DIR})

add_executable(${PROJECT_NAME} main.cc)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} pint_api)

