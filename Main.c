#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/adc.h"
#include "ff.h"
#include "f_util.h"
#include "hw_config.h"
#include "sd_card.h"


#define ADC_PIN 26
#define BOTAO_SD 5          // GPIO 5 - Apenas Desmontar SD
#define BOTAO_START_STOP 6  // GPIO 6 - Iniciar/Parar captura


static bool collecting = false;
static bool sd_mounted = false;
static absolute_time_t next_log_time;


// Funções de busca (helpers)
static sd_card_t *sd_get_by_name(const char *const name) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return sd_get_by_num(i);
    return NULL;
}

static FATFS *sd_get_fs_by_name(const char *name) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->pcName, name))
            return &sd_get_by_num(i)->fatfs;
    return NULL;
}


// Monta o cartão SD
static bool mount_sd() {
    if (sd_mounted) {
        printf("SD já está montado.\n");
        return true;
    }
    
    printf("Tentando montar SD...\n");
    
    const char *drive = sd_get_by_num(0)->pcName;
    FATFS *p_fs = sd_get_fs_by_name(drive);
    if (!p_fs) {
        printf("ERRO: Drive não encontrado.\n");
        return false;
    }
    
    FRESULT fr = f_mount(p_fs, drive, 1);
    if (FR_OK != fr) {
        printf("ERRO: Falha ao montar SD - %s\n", FRESULT_str(fr));
        printf("Verifique se o cartão está formatado (FAT32) e inserido corretamente.\n");
        return false;
    }
    
    sd_card_t *pSD = sd_get_by_name(drive);
    if (!pSD) {
        printf("ERRO: Ponteiro SD inválido.\n");
        return false;
    }
    
    pSD->mounted = true;
    sd_mounted = true;
    printf("✓ SD montado com sucesso!\n");
    return true;
}


// Desmonta o cartão SD
static void unmount_sd() {
    if (!sd_mounted) {
        printf("SD já está desmontado.\n");
        return;
    }
    
    if (collecting) {
        collecting = false;
        printf("Parando coleta para desmontar SD...\n");
        sleep_ms(100);
    }
    
    const char *drive = sd_get_by_num(0)->pcName;
    printf("Desmontando SD...\n");
    
    FRESULT fr = f_unmount(drive);
    if (fr == FR_OK) {
        sd_card_t *pSD = sd_get_by_name(drive);
        pSD->mounted = false;
        sd_mounted = false;
        printf("✓ SD desmontado com segurança. PODE REMOVER.\n");
    } else {
        printf("ERRO ao desmontar: %s\n", FRESULT_str(fr));
    }
}


// Salva os dados do ADC no arquivo
void save_adc_data() {
    static uint32_t sample_num = 0;
    
    if (!sd_mounted) {
        printf("Erro: SD não montado! Parando coleta.\n");
        collecting = false;
        return;
    }
    
    FIL file;
    // CORREÇÃO: Removido FA_CREATE_ALWAYS para permitir o append.
    // FA_OPEN_APPEND já cria o arquivo se ele não existir.
    if (f_open(&file, "dadosAgoraVai.txt", FA_WRITE | FA_OPEN_APPEND) != FR_OK) {
        printf("Erro ao abrir arquivo!\n");
        return;
    }
    
    adc_select_input(0);
    uint16_t adc_value = adc_read();
    
    char buffer[32];
    sprintf(buffer, "%lu,%d\n", ++sample_num, adc_value);
    
    f_write(&file, buffer, strlen(buffer), NULL);
    f_close(&file); // Fecha o arquivo para garantir que os dados sejam escritos
    
    printf("Amostra %lu: %d\n", sample_num, adc_value);
}


// Inicia a coleta
void start_collection() {
    if (!sd_mounted) {
        printf("ERRO: O SD não está montado!\n");
        return;
    }
    
    if (collecting) {
        printf("Coleta já está ativa.\n");
        return;
    }
    
    collecting = true;
    next_log_time = get_absolute_time();
    printf("✓ Coleta INICIADA!\n");
}

// Para a coleta
void stop_collection() {
    if (!collecting) {
        printf("Coleta já está parada.\n");
        return;
    }
    
    collecting = false;
    printf("✓ Coleta PARADA.\n");
}


// Manipulador de interrupção para os botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    static uint64_t last_press_time = 0;
    uint64_t now = time_us_64();
    
    // Debounce simples para ambos os botões
    if (now - last_press_time < 300000) return;
    last_press_time = now;
    
    if (gpio == BOTAO_SD) {
        // Botão 1: Apenas desmonta
        if (sd_mounted) {
            unmount_sd();
        }
    } else if (gpio == BOTAO_START_STOP) {
        // Botão 2: Inicia ou para a coleta
        if (collecting) {
            stop_collection();
        } else {
            start_collection();
        }
    }
}


// Mostra o status periodicamente
void show_status() {
    static uint32_t counter = 0;
    if (++counter % 50 == 0) { // A cada ~5 segundos
        printf("\n--- STATUS ---\n");
        printf("SD: %s | Coleta: %s\n",
               sd_mounted ? "MONTADO" : "DESMONTADO",
               collecting ? "ATIVA" : "PARADA");
        printf("-------------\n");
    }
}


int main() {
    stdio_init_all();
    sleep_ms(3000); // Um tempo para o monitor serial conectar
    
    printf("\n=== ADC LOGGER - Montagem Automática ===\n");
    
    // Inicializa o driver do SD Card
    if (!sd_init_driver()) {
        printf("ERRO CRÍTICO: Falha ao inicializar o driver do SD Card.\n");
        printf("VERIFIQUE AS CONEXÕES E O ARQUIVO hw_config.h\n");
        while(true); // Trava o programa
    }
    
    // Tenta montar o SD Card automaticamente
    if (!mount_sd()) {
        printf("ERRO CRÍTICO: Não foi possível montar o SD Card.\n");
        printf("O programa será interrompido.\n");
        while(true); // Trava o programa
    }

    // Configura os botões
    gpio_init(BOTAO_SD);
    gpio_set_dir(BOTAO_SD, GPIO_IN);
    gpio_pull_up(BOTAO_SD);
    
    gpio_init(BOTAO_START_STOP);
    gpio_set_dir(BOTAO_START_STOP, GPIO_IN);
    gpio_pull_up(BOTAO_START_STOP);
    
    // Configura as interrupções dos botões
    gpio_set_irq_enabled_with_callback(BOTAO_SD, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BOTAO_START_STOP, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    
    // Configura o ADC
    adc_init();
    adc_gpio_init(ADC_PIN);
    
    // Novas instruções
    printf("\n====================================\n");
    printf("INSTRUÇÕES DE OPERAÇÃO:\n");
    printf("1. O SD Card já foi montado.\n");
    printf("2. Pressione o Botão 2 (GPIO %d) para INICIAR/PARAR a coleta de dados.\n", BOTAO_START_STOP);
    printf("3. Pressione o Botão 1 (GPIO %d) para DESMONTAR o SD em segurança ANTES de removê-lo.\n", BOTAO_SD);
    printf("====================================\n\n");
    printf("Aguardando comandos...\n");
    
    // Loop principal infinito
    while (true) {
        if (collecting && sd_mounted) {
            if (time_reached(next_log_time)) {
                next_log_time = make_timeout_time_ms(1000); // Próximo log em 1 segundo
                save_adc_data();
            }
        }
        
        show_status();
        sleep_ms(100);
    }
    
    return 0;
}
