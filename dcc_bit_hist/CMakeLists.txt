
add_compile_options(-Wall -Wextra -Werror)

add_executable(dcc_bit_hist
    dcc_bit_hist.cpp
)

pico_enable_stdio_uart(dcc_bit_hist 0)
pico_enable_stdio_usb(dcc_bit_hist 1)

target_include_directories(dcc_bit_hist PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(dcc_bit_hist PRIVATE
    pico_stdlib
    pico_stdio_usb
    misc
    pio_edges
    dcc
)

pico_add_extra_outputs(dcc_bit_hist)
