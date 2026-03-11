
add_compile_options(-Wall -Wextra -Werror)

add_executable(dcc_raw
    dcc_raw.cpp
)

pico_enable_stdio_uart(dcc_raw 0)
pico_enable_stdio_usb(dcc_raw 1)

target_include_directories(dcc_raw PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(dcc_raw PRIVATE
    pico_stdlib
    pico_stdio_usb
    misc
    dcc
)

pico_add_extra_outputs(dcc_raw)
