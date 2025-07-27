# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)

# Configuração básica do projeto
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)
project(Datalogger_Acelerometro C CXX ASM)
pico_sdk_init()

# Criação do executável com todos os arquivos fonte
add_executable(${PROJECT_NAME}
    Main.c
    lib/hw_config.c
    lib/mpu6050.c
    
    # FatFs_SPI
    lib/FatFs_SPI/ff15/source/ff.c
    lib/FatFs_SPI/ff15/source/ffsystem.c
    lib/FatFs_SPI/ff15/source/ffunicode.c
    lib/FatFs_SPI/sd_driver/sd_card.c
    lib/FatFs_SPI/sd_driver/sd_spi.c
    lib/FatFs_SPI/sd_driver/spi.c
    lib/FatFs_SPI/sd_driver/crc.c
    lib/FatFs_SPI/src/glue.c
    lib/FatFs_SPI/src/f_util.c
    lib/FatFs_SPI/src/ff_stdio.c
    lib/FatFs_SPI/src/my_debug.c
    lib/FatFs_SPI/src/rtc.c
    
    # Display
    lib/Display_Bibliotecas/ssd1306.c
    
    # Matriz de LEDs
    lib/Matriz_Bibliotecas/matriz_led.c
)

# Geração do cabeçalho PIO para WS2812
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/lib/Matriz_Bibliotecas/ws2812.pio)

# Diretórios de inclusão
target_include_directories(${PROJECT_NAME} PRIVATE
    .
    lib
    lib/FatFs_SPI/ff15/source
    lib/FatFs_SPI/include
    lib/FatFs_SPI/sd_driver
    lib/FatFs_SPI/src
    lib/Display_Bibliotecas
    lib/Matriz_Bibliotecas
)

# Bibliotecas necessárias
target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_stdlib
    hardware_spi
    hardware_dma
    hardware_rtc
    hardware_i2c
    hardware_clocks
    hardware_adc
    hardware_pio
    hardware_gpio
    pico_bootrom
    hardware_pwm
)

# Configuração de saída
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_add_extra_outputs(${PROJECT_NAME})
