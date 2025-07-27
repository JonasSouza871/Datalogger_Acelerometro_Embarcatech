/********************************************************************
*  Datalogger MPU6050 + SD Card + LED RGB + OLED SSD1306 (Raspberry Pi Pico)
*  ---------------------------------------------------------------
*  ▸ I²C0  (GP0 = SDA, GP1 = SCL) → MPU6050
*  ▸ I²C1  (GP14 = SDA, GP15 = SCL) → OLED SSD1306 (128×64, addr 0x3C)
*  ▸ BOTÃO_COLETA  = GP6   ▸ BOTÃO_DESMONTAR = GP5
*  ▸ LED_RGB: R → GP13  |  G → GP11  |  B → GP12
********************************************************************/

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
#include "ssd1306.h"

/* ---------- DEFINIÇÕES DE HARDWARE ---------- */
#define I2C_MPU_PORT     i2c0
#define I2C_MPU_SDA      0
#define I2C_MPU_SCL      1

#define I2C_OLED_PORT    i2c1
#define I2C_OLED_SDA     14
#define I2C_OLED_SCL     15
#define OLED_ADDR        0x3C   /* troque se necessário */

#define BOTAO_DESMONTAR  5
#define BOTAO_COLETA     6
#define INTERVALO_COLETA_MS 1000
#define DEBOUNCE_US      300000

#define LED_GREEN_PIN    11
#define LED_BLUE_PIN     12
#define LED_RED_PIN      13

/* ---------- VARIÁVEIS GLOBAIS ---------- */
static bool              coletando      = false;
static bool              sd_montado     = false;
static absolute_time_t   proximo_log;
static uint32_t          numero_amostra = 0;
static ssd1306_t         oled;                  // display global

/* ---------- PROTÓTIPOS ---------- */
void parar_coleta(void);

/* ---------- LED RGB ---------- */
void led_init(void) {
    gpio_init(LED_RED_PIN);   gpio_set_dir(LED_RED_PIN,   GPIO_OUT);
    gpio_init(LED_GREEN_PIN); gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_init(LED_BLUE_PIN);  gpio_set_dir(LED_BLUE_PIN,  GPIO_OUT);
}

void led_set_color(bool r, bool g, bool b) {
    gpio_put(LED_RED_PIN,   r);
    gpio_put(LED_GREEN_PIN, g);
    gpio_put(LED_BLUE_PIN,  b);
}

void led_error_blink(void) {
    while (true) {
        led_set_color(true, false, true);  // roxo
        sleep_ms(250);
        led_set_color(false, false, false);
        sleep_ms(250);
    }
}

/* ---------- OLED: inicialização e helpers ---------- */
static void oled_init(void) {
    i2c_init(I2C_OLED_PORT, 400 * 1000);
    gpio_set_function(I2C_OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_OLED_SDA);
    gpio_pull_up(I2C_OLED_SCL);

    ssd1306_init(&oled, 128, 64, false, OLED_ADDR, I2C_OLED_PORT);
    ssd1306_config(&oled);
    ssd1306_fill(&oled, false);
    ssd1306_send_data(&oled);
}

static void oled_status(const char *status) {
    ssd1306_fill(&oled, false);
    ssd1306_draw_string(&oled, "Status:", 0, 0, false);
    ssd1306_draw_string(&oled, status, 0, 10, false);
    ssd1306_send_data(&oled);
}

static void oled_amostras(uint32_t n) {
    char buf[24];
    snprintf(buf, sizeof(buf), "Amostras:%lu", n);
    ssd1306_draw_string(&oled, buf, 0, 20, false);
    ssd1306_send_data(&oled);
}

static void oled_feedback(const char *msg) {
    ssd1306_draw_string(&oled, msg, 0, 40, false);
    ssd1306_send_data(&oled);
}

/* ---------- SD: helpers ---------- */
static sd_card_t *buscar_sd_por_nome(const char *nome) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (strcmp(sd_get_by_num(i)->pcName, nome) == 0) return sd_get_by_num(i);
    return NULL;
}
static FATFS *buscar_fs_por_nome(const char *nome) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (strcmp(sd_get_by_num(i)->pcName, nome) == 0) return &sd_get_by_num(i)->fatfs;
    return NULL;
}

/* ---------- SD: montar/desmontar ---------- */
bool montar_cartao_sd(void) {
    if (sd_montado) return true;

    const char *drive = sd_get_by_num(0)->pcName;
    FATFS *fs = buscar_fs_por_nome(drive);
    if (!fs) {
        printf("ERRO: Drive nao encontrado.\n");
        return false;
    }
    FRESULT r = f_mount(fs, drive, 1);
    if (r != FR_OK) {
        printf("ERRO ao montar SD: %s\n", FRESULT_str(r));
        return false;
    }
    buscar_sd_por_nome(drive)->mounted = true;
    sd_montado = true;
    printf("✓ SD montado.\n");
    return true;
}

void desmontar_cartao_sd(void) {
    if (!sd_montado) return;

    if (coletando) parar_coleta();

    const char *drive = sd_get_by_num(0)->pcName;
    f_unmount(drive);
    buscar_sd_por_nome(drive)->mounted = false;
    sd_montado = false;

    led_set_color(false, false, false);
    oled_status("SD desmontado");
    printf("✓ SD desmontado.\n");
}

/* ---------- CSV ---------- */
void criar_cabecalho_csv(void) {
    if (!sd_montado) return;
    FIL arq;
    if (f_open(&arq, "dados_mpu.csv", FA_WRITE | FA_CREATE_NEW) == FR_OK) {
        const char *cab =
            "Amostra,Acel_X,Acel_Y,Acel_Z,Giro_X,Giro_Y,Giro_Z,Temp\n";
        f_write(&arq, cab, strlen(cab), NULL);
        f_close(&arq);
        printf("Cabecalho CSV criado.\n");
    }
}

/* ---------- Coleta ---------- */
void salvar_amostra_mpu6050(void) {
    if (!sd_montado) {
        printf("SD nao montado!\n");
        coletando = false;
        oled_status("ERRO SD!");
        led_error_blink();
    }

    /* LED azul enquanto grava */
    led_set_color(false, false, true);

    FIL arq;
    if (f_open(&arq, "dados_mpu.csv", FA_WRITE | FA_OPEN_APPEND) != FR_OK) {
        printf("Erro ao abrir CSV!\n");
        oled_status("ERRO CSV!");
        led_error_blink();
    }

    mpu6050_data_t d;
    mpu6050_read_data(&d);

    char buf[256];
    snprintf(buf, sizeof(buf),
        "%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f\n",
        ++numero_amostra,
        d.accel_x, d.accel_y, d.accel_z,
        d.gyro_x,  d.gyro_y,  d.gyro_z,
        d.temp_c);

    f_write(&arq, buf, strlen(buf), NULL);
    f_close(&arq);

    /* LED vermelho: coleta ativa */
    led_set_color(true, false, false);
    oled_amostras(numero_amostra);
    oled_feedback("Dados salvos!");
}

void iniciar_coleta(void) {
    if (!sd_montado) { printf("SD nao montado.\n"); return; }
    if (coletando) return;
    coletando = true;
    led_set_color(true, false, false);
    oled_status("Gravando...");
    proximo_log = get_absolute_time();
    printf("✓ Coleta iniciada.\n");
}

void parar_coleta(void) {
    if (!coletando) return;
    coletando = false;
    led_set_color(false, true, false);
    oled_status("Coleta pausada");          /* ← mensagem solicitada */
    printf("✓ Coleta parada.\n");
}

/* ---------- Botões ---------- */
void manipular_botoes(uint gpio, uint32_t eventos) {
    static uint64_t ult = 0;
    uint64_t agora = time_us_64();
    if (agora - ult < DEBOUNCE_US) return;
    ult = agora;

    if (gpio == BOTAO_DESMONTAR) {
        sd_montado ? desmontar_cartao_sd() : printf("SD ja desmontado.\n");
    } else if (gpio == BOTAO_COLETA) {
        coletando ? parar_coleta() : iniciar_coleta();
    }
}

void configurar_botoes(void) {
    gpio_init(BOTAO_DESMONTAR); gpio_set_dir(BOTAO_DESMONTAR, GPIO_IN); gpio_pull_up(BOTAO_DESMONTAR);
    gpio_init(BOTAO_COLETA);    gpio_set_dir(BOTAO_COLETA,    GPIO_IN); gpio_pull_up(BOTAO_COLETA);

    gpio_set_irq_enabled_with_callback(BOTAO_DESMONTAR, GPIO_IRQ_EDGE_FALL, true, &manipular_botoes);
    gpio_set_irq_enabled_with_callback(BOTAO_COLETA,    GPIO_IRQ_EDGE_FALL, true, &manipular_botoes);
}

/* ---------- Sistema ---------- */
bool inicializar_sistema(void) {
    oled_status("Inicializando...");
    led_set_color(true, true, false); /* amarelo */

    if (!sd_init_driver()) { printf("Falha init SD driver\n"); return false; }
    if (!montar_cartao_sd()) { printf("Falha montar SD\n"); return false; }

    /* I²C0: MPU6050 */
    i2c_init(I2C_MPU_PORT, 400 * 1000);
    gpio_set_function(I2C_MPU_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_MPU_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_MPU_SDA); gpio_pull_up(I2C_MPU_SCL);

    mpu6050_init(I2C_MPU_PORT);

    configurar_botoes();
    criar_cabecalho_csv();

    led_set_color(false, true, false); /* verde */
    oled_status("Aguardando...");
    return true;
}

void mostrar_instrucoes(void) {
    printf("\n===== INSTRUCOES =====\n"
           "BOTAO %d → INICIAR/PARAR COLETA\n"
           "BOTAO %d → DESMONTAR SD\n"
           "======================\n", BOTAO_COLETA, BOTAO_DESMONTAR);
}

/* ---------- MAIN ---------- */
int main(void) {
    stdio_init_all();
    led_init();
    oled_init();

    sleep_ms(3000);  /* aguarda conexao serial */
    if (!inicializar_sistema()) {
        printf("Falha na inicializacao.\n");
        oled_status("ERRO FATAL!");
        led_error_blink();
    }

    mostrar_instrucoes();
    printf("Sistema pronto.\n");

    while (true) {
        if (coletando && sd_montado && time_reached(proximo_log)) {
            proximo_log = make_timeout_time_ms(INTERVALO_COLETA_MS);
            salvar_amostra_mpu6050();
        }
        sleep_ms(10);
    }
}
