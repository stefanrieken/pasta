cmake_minimum_required(VERSION 3.13)

include(../pico-sdk/pico_sdk_init.cmake)
add_compile_definitions(PICO_SDK)
add_compile_definitions(ANSI_TERM)
add_compile_definitions(LEXICAL_SCOPING)
add_compile_definitions(ANALYZE_VARS)
#add_compile_definitions(AUTO_BIND)
pico_sdk_init()

project(bianco)
add_executable(bianco src/terminal.c src/vars.c src/pasta.c src/parse.c src/run.c src/stack.c src/base.c src/int.c src/file.c src/ports/thumby.c src/ssd1306.c)
target_link_libraries(bianco pico_stdlib hardware_pwm hardware_spi ${CMAKE_CURRENT_SOURCE_DIR}/recipes/lib.pasta.obj)
pico_enable_stdio_usb(bianco 1)
pico_enable_stdio_uart(bianco 0)
# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(bianco)

project(tricolore)
add_executable(tricolore src/terminal.c src/vars.c src/pasta.c src/parse.c src/stack.c src/base.c src/int.c src/file.c src/bitmap.c src/tricolore.c src/ports/thumby_color.c src/gc9107.c)
target_link_libraries(
        tricolore
        pico_stdlib hardware_pwm hardware_spi hardware_pio hardware_gpio hardware_dma
        ${CMAKE_CURRENT_SOURCE_DIR}/hello.ram.obj
        ${CMAKE_CURRENT_SOURCE_DIR}/recipes/lib.pasta.obj
        ${CMAKE_CURRENT_SOURCE_DIR}/recipes/lib.trico.obj
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/ascii1.bmp.obj
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/ascii2.bmp.obj
)
pico_enable_stdio_usb(tricolore 1)
pico_enable_stdio_uart(tricolore 0)
# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(tricolore)

pico_generate_pio_header(tricolore ../src/pwm.pio)
