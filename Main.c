#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/adc.h"
#include "hardware/i2c.h" // Necessário para I2C
#include "ff.h"
#include "f_util.h"
#include "hw_config.h"
#include "sd_card.h"

// INCLUI A NOVA BIBLIOTECA DO MPU6050
#include "mpu6050.h"

// =============== CONFIGURAÇÕES DO SISTEMA ===============
// Pinos I2C para o MPU6050
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1

// Pinos e configurações do Logger
#define PINO_ADC 26
#define BOTAO_DESMONTAR 5
#define BOTAO_COLETA 6
#define INTERVALO_COLETA_MS 1000
#define DEBOUNCE_US 300000

// =============== VARIÁVEIS GLOBAIS ===============
static bool coletando = false;
static bool sd_montado = false;
static absolute_time_t proximo_log;
static uint32_t numero_amostra = 0;

// Declaração antecipada de funções
bool montar_cartao_sd();
void desmontar_cartao_sd();
void iniciar_coleta();
void parar_coleta();

// ... (todas as suas funções de buscar_sd, buscar_fs, etc. permanecem aqui sem alterações) ...
// Para economizar espaço, elas não foram repetidas aqui, mas devem estar no seu arquivo.

/**
 * Busca cartão SD pelo nome
 */
static sd_card_t* buscar_sd_por_nome(const char *nome) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (strcmp(sd_get_by_num(i)->pcName, nome) == 0)
            return sd_get_by_num(i);
    return NULL;
}

/**
 * Busca sistema de arquivos pelo nome
 */
static FATFS* buscar_fs_por_nome(const char *nome) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (strcmp(sd_get_by_num(i)->pcName, nome) == 0)
            return &sd_get_by_num(i)->fatfs;
    return NULL;
}

// =============== OPERAÇÕES DO CARTÃO SD ===============
// (Suas funções montar_cartao_sd e desmontar_cartao_sd permanecem aqui)
bool montar_cartao_sd() {
    if (sd_montado) {
        printf("SD já está montado.\n");
        return true;
    }
    printf("Montando cartão SD...\n");
    const char *drive = sd_get_by_num(0)->pcName;
    FATFS *fs = buscar_fs_por_nome(drive);
    if (!fs) {
        printf("ERRO: Drive não encontrado.\n");
        return false;
    }
    FRESULT resultado = f_mount(fs, drive, 1);
    if (resultado != FR_OK) {
        printf("ERRO: Falha ao montar SD - %s\n", FRESULT_str(resultado));
        return false;
    }
    sd_card_t *cartao = buscar_sd_por_nome(drive);
    cartao->mounted = true;
    sd_montado = true;
    printf("✓ SD montado com sucesso!\n");
    return true;
}

void desmontar_cartao_sd() {
    if (!sd_montado) return;
    if (coletando) {
        coletando = false;
        printf("Parando coleta para desmontar SD...\n");
        sleep_ms(100);
    }
    const char *drive = sd_get_by_num(0)->pcName;
    printf("Desmontando SD...\n");
    f_unmount(drive);
    sd_card_t *cartao = buscar_sd_por_nome(drive);
    cartao->mounted = false;
    sd_montado = false;
    printf("✓ SD desmontado com segurança.\n");
}


/**
 * @brief Salva uma amostra dos sensores (ADC e MPU6050) no arquivo
 */
void salvar_amostra_sensores() {
    if (!sd_montado) {
        printf("Erro: SD não montado! Parando coleta.\n");
        coletando = false;
        return;
    }

    // Abre arquivo para escrita (modo append)
    FIL arquivo;
    if (f_open(&arquivo, "dados_sensores.csv", FA_WRITE | FA_OPEN_APPEND) != FR_OK) {
        printf("Erro ao abrir arquivo!\n");
        return;
    }

    // Lê valor do ADC
    adc_select_input(0);
    uint16_t valor_adc = adc_read();

    // Lê dados do MPU6050 usando a biblioteca
    mpu6050_data_t mpu_data;
    mpu6050_read_data(&mpu_data);

    // Formata e escreve dados no formato CSV
    // Colunas: Amostra, ADC, AccX, AccY, AccZ, GyroX, GyroY, GyroZ, TempC
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%lu,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f\n",
             ++numero_amostra,
             valor_adc,
             mpu_data.accel_x, mpu_data.accel_y, mpu_data.accel_z,
             mpu_data.gyro_x, mpu_data.gyro_y, mpu_data.gyro_z,
             mpu_data.temp_c);
             
    f_write(&arquivo, buffer, strlen(buffer), NULL);
    f_close(&arquivo);

    printf("Amostra %lu: ADC=%d, AccX=%.2f, GyroX=%.2f\n", 
           numero_amostra, valor_adc, mpu_data.accel_x, mpu_data.gyro_x);
}

void criar_cabecalho_csv() {
    if (!sd_montado) return;
    FIL arquivo;
    if (f_open(&arquivo, "dados_sensores.csv", FA_WRITE | FA_CREATE_NEW) == FR_OK) {
        const char* cabecalho = "Amostra,Valor_ADC,Acel_X(m/s^2),Acel_Y(m/s^2),Acel_Z(m/s^2),Giro_X(o/s),Giro_Y(o/s),Giro_Z(o/s),Temperatura(C)\n";
        f_write(&arquivo, cabecalho, strlen(cabecalho), NULL);
        f_close(&arquivo);
        printf("Arquivo 'dados_sensores.csv' criado com cabeçalho.\n");
    }
}


// ... (suas funções iniciar/parar coleta e de botões permanecem aqui) ...
void iniciar_coleta() {
    if (!sd_montado) {
        printf("ERRO: O SD não está montado!\n");
        return;
    }
    if (coletando) return;
    coletando = true;
    proximo_log = get_absolute_time();
    printf("✓ Coleta INICIADA!\n");
}

void parar_coleta() {
    if (!coletando) return;
    coletando = false;
    printf("✓ Coleta PARADA.\n");
}

void manipular_botoes(uint gpio, uint32_t eventos) {
    static uint64_t ultimo_clique = 0;
    uint64_t agora = time_us_64();
    if (agora - ultimo_clique < DEBOUNCE_US) return;
    ultimo_clique = agora;
    
    switch (gpio) {
        case BOTAO_DESMONTAR:
            if (sd_montado) desmontar_cartao_sd();
            break;
        case BOTAO_COLETA:
            coletando ? parar_coleta() : iniciar_coleta();
            break;
    }
}

void configurar_botoes() {
    gpio_init(BOTAO_DESMONTAR);
    gpio_set_dir(BOTAO_DESMONTAR, GPIO_IN);
    gpio_pull_up(BOTAO_DESMONTAR);
    
    gpio_init(BOTAO_COLETA);
    gpio_set_dir(BOTAO_COLETA, GPIO_IN);
    gpio_pull_up(BOTAO_COLETA);
    
    gpio_set_irq_enabled_with_callback(BOTAO_DESMONTAR, GPIO_IRQ_EDGE_FALL, true, &manipular_botoes);
    gpio_set_irq_enabled_with_callback(BOTAO_COLETA, GPIO_IRQ_EDGE_FALL, true, &manipular_botoes);
}

/**
 * @brief Inicializa todo o sistema
 */
bool inicializar_sistema() {
    printf("\n=== LOGGER Multi-Sensor - Sistema Iniciando ===\n");

    // Inicializa driver do SD Card
    if (!sd_init_driver()) {
        printf("ERRO CRÍTICO: Falha ao inicializar driver do SD Card.\n");
        return false;
    }

    // Monta cartão SD
    if (!montar_cartao_sd()) {
        printf("ERRO CRÍTICO: Não foi possível montar o SD Card.\n");
        return false;
    }

    // Configura I2C para o MPU6050
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    printf("I2C configurado nos pinos SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);

    // Inicializa o MPU6050 usando a biblioteca
    mpu6050_init(I2C_PORT);

    // Configura ADC
    adc_init();
    adc_gpio_init(PINO_ADC);

    // Configura botões
    configurar_botoes();
    
    // Cria o arquivo CSV com cabeçalho se ele não existir
    criar_cabecalho_csv();

    return true;
}

// ... (sua função de mostrar instruções e status permanece aqui) ...
void mostrar_instrucoes() {
    printf("\n====================================\n");
    printf("INSTRUÇÕES DE OPERAÇÃO:\n");
    printf("1. Botão Coleta (GPIO %d): INICIAR/PARAR\n", BOTAO_COLETA);
    printf("2. Botão Desmontagem (GPIO %d): Desmontar SD\n", BOTAO_DESMONTAR);
    printf("====================================\n\n");
}


// =============== PROGRAMA PRINCIPAL ===============
int main() {
    stdio_init_all();
    sleep_ms(3000);

    if (!inicializar_sistema()) {
        printf("Falha na inicialização. Sistema interrompido.\n");
        while (true) sleep_ms(1000);
    }

    mostrar_instrucoes();
    printf("Sistema pronto. Aguardando comandos...\n");

    while (true) {
        if (coletando && sd_montado && time_reached(proximo_log)) {
            proximo_log = make_timeout_time_ms(INTERVALO_COLETA_MS);
            // Chama a nova função para salvar os dados de todos os sensores
            salvar_amostra_sensores();
        }
        sleep_ms(10); // Pequeno delay para não sobrecarregar o processador
    }
}
