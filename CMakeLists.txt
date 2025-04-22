cmake_minimum_required(VERSION 3.13)

include(../pico-sdk/pico_sdk_init.cmake)
add_compile_definitions(PICO_SDK)
pico_sdk_init()

project(bianco)
add_executable(bianco src/main.c src/pasta.c src/stack.c src/base.c src/int.c src/chef.c src/thumby.c src/ssd1306.c)
target_link_libraries(bianco pico_stdlib hardware_pwm hardware_spi)
pico_enable_stdio_usb(bianco 1)
pico_enable_stdio_uart(bianco 0)
# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(bianco)

project(thrico)
add_executable(thrico src/main.c src/pasta.c src/stack.c src/base.c src/int.c src/chef.c src/thumby.c src/gc9107.c)
target_link_libraries(thrico pico_stdlib hardware_pwm hardware_spi hardware_pio hardware_gpio hardware_dma)
pico_enable_stdio_usb(thrico 1)
pico_enable_stdio_uart(thrico 0)
# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(thrico)

pico_generate_pio_header(thrico ../src/pwm.pio)
