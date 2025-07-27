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

# --- Configuração do Projeto e SDK ---
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")  # Mude para 'pico' se não for Pico W

# Inclui o Pico SDK
include(pico_sdk_import.cmake)

project(Datalogger_Acelerometro C CXX ASM)

# Inicializa o Pico SDK
pico_sdk_init()

# --- Definição do Executável Principal ---
# Criamos o executável PRIMEIRO, com TODOS os ficheiros de código-fonte (.c)
add_executable(${PROJECT_NAME}
    # Ficheiros principais
    Main.c
    lib/hw_config.c

    # --- BIBLIOTECA MPU6050 ---
    lib/mpu6050.c

    # Ficheiros da biblioteca FatFs_SPI
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

    # Ficheiros da biblioteca do Display
    lib/Display_Bibliotecas/ssd1306.c

    # Ficheiros da biblioteca da Matriz de LEDs
    lib/Matriz_Bibliotecas/matriz_led.c
)

# --- Geração de Código PIO ---
# Agora que o target existe, podemos gerar o cabeçalho PIO
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/lib/Matriz_Bibliotecas/ws2812.pio)

# --- Adicionar TODOS os Diretórios de Inclusão (.h) ---
target_include_directories(${PROJECT_NAME} PRIVATE
    .   # Diretório raiz do projeto
    lib # Diretório para a biblioteca MPU6050 e outras

    # Diretórios da biblioteca FatFs_SPI
    lib/FatFs_SPI/ff15/source
    lib/FatFs_SPI/include
    lib/FatFs_SPI/sd_driver
    lib/FatFs_SPI/src

    # Diretórios das outras bibliotecas
    lib/Display_Bibliotecas
    lib/Matriz_Bibliotecas
)

# --- Vincular TODAS as Bibliotecas Necessárias ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_stdlib      # Stdlib básica (inclui GPIO, etc.)
    hardware_spi     # Para SPI (usado em SD card)
    hardware_dma     # Para DMA
    hardware_rtc     # Para RTC
    hardware_i2c     # Para I2C (usado em display e MPU6050)
    hardware_clocks  # Para clocks
    hardware_adc     # Para ADC
    hardware_pio     # Para PIO (ws2812)
    hardware_gpio    # Para GPIOs e IRQs (botões)
    pico_bootrom     # Para reset_usb_boot
)

# --- Configuração de Saída Padrão (stdio) e Ficheiros Finais ---
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_add_extra_outputs(${PROJECT_NAME})
