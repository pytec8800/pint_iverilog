cmake_minimum_required(VERSION 2.8)
project(pint_simu_stub)

set(PINT_INC_INSTALL_DIR /usr/local/pint/include /usr/local/pint/include/uap_API)
set(PINT_LIB_INSTALL_DIR /usr/local/pint/lib)

if(SIMU_RECORD)
    add_definitions("-DSIMU_RECORD")
endif()
add_compile_options(-g)
include_directories(${PINT_INC_INSTALL_DIR})
link_directories(${PINT_LIB_INSTALL_DIR})
add_executable(${PROJECT_NAME} pint_api_test.c)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} pint_api)


