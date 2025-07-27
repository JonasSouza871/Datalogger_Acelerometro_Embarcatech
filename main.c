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

static ssd1306_t         oled;               /* display */
static char              g_status[18]  = "INICIALIZ.";
static char              g_msg[18]     = "";
static uint32_t          g_amostras    = 0;

/* ---------- LED RGB ---------- */
static void led_init(void) {
    gpio_init(LED_RED_PIN);   gpio_set_dir(LED_RED_PIN,   GPIO_OUT);
    gpio_init(LED_GREEN_PIN); gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_init(LED_BLUE_PIN);  gpio_set_dir(LED_BLUE_PIN,  GPIO_OUT);
}
static void led_set_color(bool r, bool g, bool b) {
    gpio_put(LED_RED_PIN,   r);
    gpio_put(LED_GREEN_PIN, g);
    gpio_put(LED_BLUE_PIN,  b);
}
static void led_error_blink(void) {
    while (1) {
        led_set_color(true, false, true);  /* roxo */
        sleep_ms(250);
        led_set_color(false, false, false);
        sleep_ms(250);
    }
}

/* ---------- OLED: inicialização e UI ---------- */
static void oled_init(void) {
    i2c_init(I2C_OLED_PORT, 400 * 1000);
    gpio_set_function(I2C_OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_OLED_SDA);
    gpio_pull_up(I2C_OLED_SCL);

    ssd1306_init(&oled, 128, 64, false, OLED_ADDR, I2C_OLED_PORT);
    ssd1306_config(&oled);
}

/* --- helpers para UI --- */
static void ui_redraw(void)
{
    ssd1306_fill(&oled, false);                    /* limpa tudo            */

    /* cabeçalho */
   
    ssd1306_draw_string(&oled, "MPU6050 LOGGER", 14, 1, false);

    /* divisórias horizontais */
    ssd1306_hline(&oled, 0, 127, 12, true);
    ssd1306_hline(&oled, 0, 127, 30, true);
    ssd1306_hline(&oled, 0, 127, 48, true);

    /* STATUS (linha 16) */
    char buf_status[30];
    snprintf(buf_status, sizeof(buf_status), "STATUS: %s", g_status);
    ssd1306_draw_string(&oled, buf_status, 0, 16, false);

    /* AMOSTRAS (linha 34) */
    char buf_amostra[30];
    snprintf(buf_amostra, sizeof(buf_amostra), "AMOSTRAS: %lu", g_amostras);
    ssd1306_draw_string(&oled, buf_amostra, 0, 34, false);

    /* Mensagens / rodapé (linha 52) */
    ssd1306_draw_string(&oled, g_msg, 0, 52, false);

    ssd1306_send_data(&oled);
}

/* setters amigáveis ------------------------------------------------ */
static void ui_set_status(const char *txt)  { strncpy(g_status, txt, sizeof g_status - 1); ui_redraw(); }
static void ui_set_msg(const char *txt)     { strncpy(g_msg,    txt, sizeof g_msg    - 1); ui_redraw(); }
static void ui_set_amostras(uint32_t n)     { g_amostras = n; ui_redraw(); }

/* ---------- SD helpers ---------- */
static sd_card_t *buscar_sd_por_nome(const char *nome) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (!strcmp(sd_get_by_num(i)->pcName, nome))
            return sd_get_by_num(i);
    return NULL;
}
static FATFS *buscar_fs_por_nome(const char *nome) {
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (!strcmp(sd_get_by_num(i)->pcName, nome))
            return &sd_get_by_num(i)->fatfs;
    return NULL;
}

/* ---------- SD: montar / desmontar ---------- */
static bool montar_cartao_sd(void) {
    if (sd_montado) return true;

    const char *drive = sd_get_by_num(0)->pcName;
    FATFS *fs = buscar_fs_por_nome(drive);
    if (!fs) { printf("Drive nao encontrado.\n"); return false; }

    FRESULT r = f_mount(fs, drive, 1);
    if (r != FR_OK) { printf("f_mount: %s\n", FRESULT_str(r)); return false; }

    buscar_sd_por_nome(drive)->mounted = true;
    sd_montado = true;
    printf("SD montado.\n");
    return true;
}

static void desmontar_cartao_sd(void) {
    if (!sd_montado) return;

    if (coletando) { /* garante parada limpa */
        coletando = false;
        led_set_color(false, true, false);
    }

    const char *drive = sd_get_by_num(0)->pcName;
    f_unmount(drive);
    buscar_sd_por_nome(drive)->mounted = false;
    sd_montado = false;

    led_set_color(false, false, false);
    ui_set_status("SD OFF");
    ui_set_msg("");
    printf("SD desmontado.\n");
}

/* ---------- CSV ---------- */
static void criar_cabecalho_csv(void) {
    if (!sd_montado) return;
    FIL arq;
    if (f_open(&arq, "dados_mpu.csv", FA_WRITE | FA_CREATE_NEW) == FR_OK) {
        const char *cab =
            "Amostra,Acel_X,Acel_Y,Acel_Z,Giro_X,Giro_Y,Giro_Z,Temp\n";
        f_write(&arq, cab, strlen(cab), NULL);
        f_close(&arq);
        printf("CSV criado.\n");
    }
}

/* ---------- Coleta ---------- */
static void salvar_amostra_mpu6050(void) {
    if (!sd_montado) {
        ui_set_status("ERRO SD");
        led_error_blink();
    }

    led_set_color(false, false, true);             /* LED azul = gravação */

    FIL arq;
    if (f_open(&arq, "dados_mpu.csv", FA_WRITE | FA_OPEN_APPEND) != FR_OK) {
        ui_set_status("ERRO CSV");
        led_error_blink();
    }

    mpu6050_data_t d; mpu6050_read_data(&d);

    char buf[256];
    snprintf(buf, sizeof(buf),
        "%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f\n",
        ++numero_amostra,
        d.accel_x, d.accel_y, d.accel_z,
        d.gyro_x,  d.gyro_y,  d.gyro_z,
        d.temp_c);

    f_write(&arq, buf, strlen(buf), NULL);
    f_close(&arq);

    led_set_color(true, false, false);             /* LED vermelho = ativo */

    ui_set_amostras(numero_amostra);
    ui_set_msg("Dados salvos");
}

static void iniciar_coleta(void) {
    if (!sd_montado) { printf("SD nao montado.\n"); return; }
    if (coletando) return;

    coletando = true;
    led_set_color(true, false, false);
    ui_set_status("GRAVANDO");
    ui_set_msg("");
    proximo_log = get_absolute_time();
}

static void parar_coleta(void) {
    if (!coletando) return;

    coletando = false;
    led_set_color(false, true, false);
    ui_set_status("PAUSADA");
    ui_set_msg("");
}

/* ---------- Botões ---------- */
static void gpio_callback(uint gpio, uint32_t events) {
    static uint64_t ult = 0;
    uint64_t agora = time_us_64();
    if (agora - ult < DEBOUNCE_US) return;
    ult = agora;

    if (gpio == BOTAO_DESMONTAR)
        sd_montado ? desmontar_cartao_sd() : montar_cartao_sd();
    else if (gpio == BOTAO_COLETA)
        coletando ? parar_coleta() : iniciar_coleta();
}

static void configurar_botoes(void) {
    gpio_init(BOTAO_DESMONTAR); gpio_set_dir(BOTAO_DESMONTAR, GPIO_IN); gpio_pull_up(BOTAO_DESMONTAR);
    gpio_init(BOTAO_COLETA);    gpio_set_dir(BOTAO_COLETA,    GPIO_IN); gpio_pull_up(BOTAO_COLETA);

    gpio_set_irq_enabled_with_callback(BOTAO_DESMONTAR, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BOTAO_COLETA,    GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
}

/* ---------- Sistema ---------- */
static bool inicializar_sistema(void) {
    ui_set_status("INICIALIZ.");
    led_set_color(true, true, false);

    if (!sd_init_driver())          { printf("SD driver.\n"); return false; }
    if (!montar_cartao_sd())        { printf("Montar SD.\n"); return false; }

    /* I²C0: MPU6050 */
    i2c_init(I2C_MPU_PORT, 400*1000);
    gpio_set_function(I2C_MPU_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_MPU_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_MPU_SDA); gpio_pull_up(I2C_MPU_SCL);

    mpu6050_init(I2C_MPU_PORT);

    configurar_botoes();
    criar_cabecalho_csv();

    led_set_color(false, true, false);
    ui_set_status("PRONTO");
    return true;
}

/* ---------- MAIN ---------- */
int main(void)
{
    stdio_init_all();
    led_init();
    oled_init();
    ui_redraw();                                   /* tela inicial          */

    sleep_ms(2500);                                /* aguarda terminal      */
    if (!inicializar_sistema()) {
        ui_set_status("ERRO FATAL");
        led_error_blink();
    }

    while (1) {
        if (coletando && sd_montado && time_reached(proximo_log)) {
            proximo_log = make_timeout_time_ms(INTERVALO_COLETA_MS);
            salvar_amostra_mpu6050();
        }
        sleep_ms(10);
    }
}
