cmake_minimum_required(VERSION 3.13)
project(dcc_native_tests CXX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Wextra -Werror)

# Stub headers override Pico SDK includes
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/stubs)

# Library headers
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../misc/include
)

add_executable(dcc_tests
    test_main.cpp
    test_dcc_pkt.cpp
    test_dcc_bit.cpp
    # DCC sources
    ../src/dcc_pkt.cpp
    ../src/dcc_bit.cpp
    ../src/dcc_pkt2.cpp
    ../src/dcc_loco.cpp
    ../src/railcom_msg.cpp
    ../src/railcom_spec.cpp
    # Misc sources
    ../../misc/src/str_ops.c
    ../../misc/src/argv.cpp
    ../../misc/src/buf_log.cpp
    ../../misc/src/dump.cpp
)

enable_testing()
add_test(NAME dcc_tests COMMAND dcc_tests)
