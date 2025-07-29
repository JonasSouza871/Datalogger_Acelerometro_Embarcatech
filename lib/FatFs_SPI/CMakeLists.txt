
add_library(FatFs_SPI INTERFACE)

# Adiciona os arquivos de código-fonte (.c) necessários para a biblioteca.
target_sources(FatFs_SPI INTERFACE
    # --- Fontes da biblioteca FatFs ---
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffunicode.c

    # --- Fontes do seu driver de SD Card ---i.
    ${CMAKE_SOURCE_DIR}/sd_driver/sd_card.c
    ${CMAKE_SOURCE_DIR}/sd_driver/sd_spi.c
    ${CMAKE_SOURCE_DIR}/sd_driver/spi.c
    ${CMAKE_SOURCE_DIR}/sd_driver/crc.c
    ${CMAKE_SOURCE_DIR}/src/glue.c
    ${CMAKE_SOURCE_DIR}/src/f_util.c
    ${CMAKE_SOURCE_DIR}/src/ff_stdio.c
    ${CMAKE_SOURCE_DIR}/src/my_debug.c
    ${CMAKE_SOURCE_DIR}/src/rtc.c
)

# Adiciona os diretórios de include para que o compilador encontre os arquivos .h
target_include_directories(FatFs_SPI INTERFACE
    # Diretórios relativos da biblioteca
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_SOURCE_DIR}/sd_driver
    ${CMAKE_SOURCE_DIR}/src
)

# Adiciona as bibliotecas do Pico SDK e a nossa biblioteca de configuração de hardware.
target_link_libraries(FatFs_SPI INTERFACE
    # Bibliotecas do SDK necessárias para o SD card
    hardware_spi
    hardware_dma
    hardware_rtc
    pico_stdlib
    HardwareConfig
)
