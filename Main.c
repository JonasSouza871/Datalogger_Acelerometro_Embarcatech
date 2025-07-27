#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/i2c.h"
#include "ff.h"
#include "f_util.h"
#include "hw_config.h"
#include "sd_card.h"
#include "mpu6050.h"

// =============== CONFIGURAÇÕES DO SISTEMA ===============
// Pinos I2C para o MPU6050
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1

// Pinos e configurações do Logger
#define BOTAO_DESMONTAR 5
#define BOTAO_COLETA 6
#define INTERVALO_COLETA_MS 1000
#define DEBOUNCE_US 300000

// NOVO: Definição dos pinos do LED RGB
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12
#define LED_RED_PIN 13

// =============== VARIÁVEIS GLOBAIS ===============
static bool coletando = false;
static bool sd_montado = false;
static absolute_time_t proximo_log;
static uint32_t numero_amostra = 0;

// =============== PROTÓTIPOS DE FUNÇÕES ===============
// Declaração antecipada para resolver o aviso de "implicit declaration",
// já que desmontar_cartao_sd() é definida antes de parar_coleta().
void parar_coleta();


// =============== NOVAS FUNÇÕES DE CONTROLE DO LED ===============

/**
 * @brief Inicializa os pinos do LED RGB como saídas.
 */
void led_init() {
    gpio_init(LED_RED_PIN);
    gpio_init(LED_GREEN_PIN);
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
}

/**
 * @brief Define a cor do LED.
 * Assume LED de catodo comum (1 = LIGADO, 0 = DESLIGADO).
 * @param r true para ligar o vermelho
 * @param g true para ligar o verde
 * @param b true para ligar o azul
 */
void led_set_color(bool r, bool g, bool b) {
    gpio_put(LED_RED_PIN, r);
    gpio_put(LED_GREEN_PIN, g);
    gpio_put(LED_BLUE_PIN, b);
}

/**
 * @brief Pisca o LED na cor roxa para indicar um erro fatal.
 * Esta função entra em um loop infinito.
 */
void led_error_blink() {
    while (true) {
        led_set_color(true, false, true); // Roxo
        sleep_ms(250);
        led_set_color(false, false, false); // Desligado
        sleep_ms(250);
    }
}

// Funções auxiliares para o SD Card (devem estar no seu código)
static sd_card_t* buscar_sd_por_nome(const char *nome) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (strcmp(sd_get_by_num(i)->pcName, nome) == 0)
            return sd_get_by_num(i);
    return NULL;
}

static FATFS* buscar_fs_por_nome(const char *nome) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (strcmp(sd_get_by_num(i)->pcName, nome) == 0)
            return &sd_get_by_num(i)->fatfs;
    return NULL;
}

// =============== OPERAÇÕES DO CARTÃO SD ===============
bool montar_cartao_sd() {
    if (sd_montado) return true;
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
        parar_coleta(); // Usa a função para garantir que o LED mude de cor
    }
    const char *drive = sd_get_by_num(0)->pcName;
    printf("Desmontando SD...\n");
    f_unmount(drive);
    sd_card_t *cartao = buscar_sd_por_nome(drive);
    cartao->mounted = false;
    sd_montado = false;
    led_set_color(false, false, false); // Desliga o LED
    printf("✓ SD desmontado com segurança.\n");
}

/**
 * @brief Salva uma amostra do MPU6050 no arquivo CSV.
 */
void salvar_amostra_mpu6050() {
    if (!sd_montado) {
        printf("Erro: SD não montado! Parando coleta.\n");
        coletando = false;
        led_error_blink(); // Erro durante a coleta
        return;
    }

    // MODIFICADO: Pisca azul para indicar acesso ao SD
    led_set_color(false, false, true); // Azul
    sleep_ms(50); // Mantém azul por um instante

    FIL arquivo;
    if (f_open(&arquivo, "dados_mpu.csv", FA_WRITE | FA_OPEN_APPEND) != FR_OK) {
        printf("Erro ao abrir arquivo 'dados_mpu.csv'!\n");
        led_error_blink(); // Erro de escrita no SD
        return;
    }

    mpu6050_data_t mpu_data;
    mpu6050_read_data(&mpu_data);

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f\n",
             ++numero_amostra,
             mpu_data.accel_x, mpu_data.accel_y, mpu_data.accel_z,
             mpu_data.gyro_x, mpu_data.gyro_y, mpu_data.gyro_z,
             mpu_data.temp_c);
             
    f_write(&arquivo, buffer, strlen(buffer), NULL);
    f_close(&arquivo);

    // MODIFICADO: Restaura a cor vermelha de coleta
    led_set_color(true, false, false); // Vermelho

    printf("Amostra %lu | Acc(X,Y,Z): %.2f, %.2f, %.2f | Gyro(X,Y,Z): %.2f, %.2f, %.2f | Temp: %.2fC\n",
           numero_amostra,
           mpu_data.accel_x, mpu_data.accel_y, mpu_data.accel_z,
           mpu_data.gyro_x, mpu_data.gyro_y, mpu_data.gyro_z,
           mpu_data.temp_c);
}

void criar_cabecalho_csv() {
    if (!sd_montado) return;
    FIL arquivo;
    if (f_open(&arquivo, "dados_mpu.csv", FA_WRITE | FA_CREATE_NEW) == FR_OK) {
        const char* cabecalho = "Amostra,Acel_X(m/s^2),Acel_Y(m/s^2),Acel_Z(m/s^2),Giro_X(o/s),Giro_Y(o/s),Giro_Z(o/s),Temperatura(C)\n";
        f_write(&arquivo, cabecalho, strlen(cabecalho), NULL);
        f_close(&arquivo);
        printf("Arquivo 'dados_mpu.csv' criado com cabeçalho.\n");
    }
}

// Funções de controle (iniciar/parar coleta, botões)
void iniciar_coleta() {
    if (!sd_montado) {
        printf("ERRO: O SD não está montado!\n");
        return;
    }
    if (coletando) return;
    coletando = true;
    led_set_color(true, false, false); // MODIFICADO: Vermelho para coleta
    proximo_log = get_absolute_time();
    printf("✓ Coleta INICIADA!\n");
}

void parar_coleta() {
    if (!coletando) return;
    coletando = false;
    led_set_color(false, true, false); // MODIFICADO: Verde para pronto
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
    printf("\n=== Datalogger MPU6050 - Sistema Iniciando ===\n");
    led_set_color(true, true, false); // MODIFICADO: Amarelo para inicializando

    if (!sd_init_driver()) {
        printf("ERRO CRÍTICO: Falha ao inicializar driver do SD Card.\n");
        return false; // Retorna false para ser tratado no main
    }

    if (!montar_cartao_sd()) {
        printf("ERRO CRÍTICO: Não foi possível montar o SD Card.\n");
        return false; // Retorna false para ser tratado no main
    }

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    mpu6050_init(I2C_PORT);
    configurar_botoes();
    criar_cabecalho_csv();

    led_set_color(false, true, false); // MODIFICADO: Verde para sistema pronto
    return true;
}

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
    led_init(); // NOVO: Inicializa os pinos do LED
    sleep_ms(3000);

    if (!inicializar_sistema()) {
        printf("Falha na inicialização. Sistema interrompido.\n");
        led_error_blink(); // MODIFICADO: Pisca roxo em caso de erro fatal
    }

    mostrar_instrucoes();
    printf("Sistema pronto. Aguardando comandos...\n");

    while (true) {
        if (coletando && sd_montado && time_reached(proximo_log)) {
            proximo_log = make_timeout_time_ms(INTERVALO_COLETA_MS);
            salvar_amostra_mpu6050();
        }
        sleep_ms(10);
    }
}
