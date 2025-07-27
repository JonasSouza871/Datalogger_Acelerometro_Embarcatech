# Define a biblioteca FatFs_SPI.
# Usamos INTERFACE porque ela é uma coleção de fontes que serão adicionadas
# ao executável final, e não uma biblioteca compilada separadamente.
add_library(FatFs_SPI INTERFACE)

# Adiciona os arquivos de código-fonte (.c) necessários para a biblioteca.
target_sources(FatFs_SPI INTERFACE
    # --- Fontes da biblioteca FatFs ---
    # O caminho é relativo à pasta atual (lib/FatFs_SPI)
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffunicode.c

    # --- Fontes do seu driver de SD Card ---
    # O caminho é a partir da raiz do projeto, pois estão fora desta pasta.
    # Note que hw_config.c NÃO está aqui.
    ${CMAKE_SOURCE_DIR}/sd_driver/sd_card.c
    ${CMAKE_SOURCE_DIR}/sd_driver/sd_spi.c
    ${CMAKE_SOURCE_DIR}/sd_driver/spi.c
    ${CMAKE_SOURCE_DIR}/sd_driver/crc.c

    # --- Fontes de suporte (glue code) ---
    # Também a partir da raiz do projeto.
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

    # Diretórios da raiz do projeto
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
    

    # !! IMPORTANTE !!
    # Vincula a biblioteca HardwareConfig que definimos no CMakeLists.txt principal.
    # É isso que dá a esta biblioteca acesso às definições de pino em hw_config.h
    # e resolve o loop de compilação.
    HardwareConfig
)
