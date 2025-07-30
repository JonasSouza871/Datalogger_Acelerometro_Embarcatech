#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "ff.h"
#include "f_util.h"
#include "hw_config.h"
#include "sd_card.h"
#include "mpu6050.h"
#include "ssd1306.h"

// CONFIGURAÇÕES DE HARDWARE - Definem quais pinos usar

// Pinos do I²C para o sensor MPU6050
#define I2C_SENSOR_PORTA i2c0
#define I2C_SENSOR_SDA 0
#define I2C_SENSOR_SCL 1

// Pinos do I²C para o display OLED
#define I2C_DISPLAY_PORTA i2c1
#define I2C_DISPLAY_SDA 14
#define I2C_DISPLAY_SCL 15
#define ENDERECO_OLED 0x3C

// Pinos dos botões de controle
#define BOTAO_CARTAO_SD 5 // Liga/desliga cartão SD
#define BOTAO_GRAVACAO 6 // Inicia/para gravação
#define BOTAO_VALORES 22 // Cicla entre as telas (principal -> valores -> gráfico -> principal)

// Pinos do LED RGB para indicações visuais
#define LED_VERMELHO 13
#define LED_VERDE 11
#define LED_AZUL 12

// Pino do buzzer
#define BUZZER_PIN 10 // Buzzer conectado no pino 10

// Configurações de tempo
#define TEMPO_ENTRE_LEITURAS_MS 500 // 500 ms entre cada medição
#define TEMPO_DEBOUNCE_US 300000 // Evita múltiplos cliques nos botões
#define TEMPO_ATUALIZACAO_VALORES_MS 500 // Atualiza valores dos sensores na tela

// Configurações do buzzer (frequências alteradas para maior audibilidade)
#define FREQ_BEEP_CURTO 3500 // Frequência dos beeps curtos (3.5kHz)
#define FREQ_BEEP_LONGO 1000 // Frequência do beep longo (1.0kHz)
#define FREQ_BEEP_PRONTO 2500 // Frequência do beep de sistema pronto (2.5kHz)
#define DURACAO_BEEP_CURTO 100 // Duração do beep curto (100ms)
#define DURACAO_BEEP_LONGO 500 // Duração do beep longo (500ms)
#define DURACAO_BEEP_PRONTO 250 // Duração do beep de sistema pronto (250ms)
#define PAUSA_ENTRE_BEEPS 150 // Pausa entre beeps múltiplos (150ms)

// ENUMS E DEFINIÇÕES

// Define os tipos de tela disponíveis
typedef enum {
    TELA_PRINCIPAL = 0,
    TELA_VALORES = 1,
    TELA_GRAFICO = 2,
    TOTAL_TELAS = 3
} tipo_tela_t;

// Estados do buzzer não-bloqueante
typedef enum {
    BUZZER_IDLE = 0,
    BUZZER_BEEP_CURTO,
    BUZZER_PAUSA_DUPLO,
    BUZZER_SEGUNDO_BEEP,
    BUZZER_BEEP_LONGO,
    BUZZER_BEEP_PRONTO
} estado_buzzer_t;

// VARIÁVEIS GLOBAIS - Controlam o estado do sistema

// Estados principais do sistema
static bool esta_gravando = false;
static bool cartao_sd_conectado = false;
static absolute_time_t proxima_medicao;
static uint32_t contador_amostras = 0;

// Controle das telas do display
static tipo_tela_t tela_atual = TELA_PRINCIPAL;
static absolute_time_t proxima_atualizacao_valores;

// Variáveis do display OLED
static ssd1306_t display_oled;
static char texto_status[18] = "INICIANDO...";
static char texto_mensagem[18] = "";
static uint32_t numero_amostras_display = 0;

// Dados mais recentes do sensor para exibição
static mpu6050_data_t dados_sensor_atuais;

// Variáveis do PWM para o buzzer
static uint slice_buzzer;

// Controle não-bloqueante do buzzer
static estado_buzzer_t estado_buzzer = BUZZER_IDLE;
static absolute_time_t tempo_buzzer;
static bool eh_duplo_beep_flag = false; // Flag para controlar o beep duplo

// FUNÇÕES DO BUZZER - Indicações sonoras do sistema (NÃO-BLOQUEANTE)

// Configura o PWM para o buzzer
static void configurar_buzzer(void) {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice_buzzer = pwm_gpio_to_slice_num(BUZZER_PIN);
    // a frequência do clock do PWM. O clock do sistema é 125MHz. Dividindo por 25, 
    // temos um clock de 5MHz para o PWM, permitindo gerar uma gama maior de frequências.
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 25.0f); 
    pwm_init(slice_buzzer, &config, false); // Aplica configuração, mas não inicia

    pwm_set_chan_level(slice_buzzer, PWM_CHAN_A, 0); // Começa desligado
    pwm_set_enabled(slice_buzzer, true); // Habilita o slice do PWM
}

// Liga o buzzer com frequência específica
static void ligar_buzzer(uint32_t frequencia) {
    if (frequencia == 0) {
        pwm_set_chan_level(slice_buzzer, PWM_CHAN_A, 0);
        return;
    }
    // O clock do PWM foi dividido por 25 na configuração inicial (resultando em 5MHz).
    const float div_clock_freq = 5000000.0f;
    // O contador do PWM (wrap) é de 16 bits (0-65535).
    // A fórmula é: wrap = (clock_dividido / frequencia) - 1
    uint16_t wrap = (uint16_t)(div_clock_freq / frequencia) - 1;

    pwm_set_wrap(slice_buzzer, wrap);
    pwm_set_chan_level(slice_buzzer, PWM_CHAN_A, wrap / 2); // 50% duty cycle para volume máximo
}

// Desliga o buzzer
static void desligar_buzzer(void) {
    pwm_set_chan_level(slice_buzzer, PWM_CHAN_A, 0);
}

// Inicia um beep curto (não-bloqueante)
static void iniciar_beep_curto(void) {
    if (estado_buzzer != BUZZER_IDLE) return;
    estado_buzzer = BUZZER_BEEP_CURTO;
    tempo_buzzer = make_timeout_time_ms(DURACAO_BEEP_CURTO);
    ligar_buzzer(FREQ_BEEP_CURTO);
}

// Inicia dois beeps curtos (não-bloqueante)
static void iniciar_dois_beeps(void) {
    if (estado_buzzer != BUZZER_IDLE) return; // Não interrompe outro som
    eh_duplo_beep_flag = true; // Ativa a flag para o segundo beep
    estado_buzzer = BUZZER_BEEP_CURTO;
    tempo_buzzer = make_timeout_time_ms(DURACAO_BEEP_CURTO);
    ligar_buzzer(FREQ_BEEP_CURTO);
}

// Inicia um beep longo (não-bloqueante)
static void iniciar_beep_longo(void) {
    if (estado_buzzer != BUZZER_IDLE) return;
    estado_buzzer = BUZZER_BEEP_LONGO;
    tempo_buzzer = make_timeout_time_ms(DURACAO_BEEP_LONGO);
    ligar_buzzer(FREQ_BEEP_LONGO);
}

// Inicia o beep de "sistema pronto" (não-bloqueante)
static void iniciar_beep_pronto(void) {
    if (estado_buzzer != BUZZER_IDLE) return;
    estado_buzzer = BUZZER_BEEP_PRONTO;
    tempo_buzzer = make_timeout_time_ms(DURACAO_BEEP_PRONTO);
    ligar_buzzer(FREQ_BEEP_PRONTO);
}

// Atualiza o estado do buzzer (chamada no loop principal)
static void atualizar_buzzer(void) {
    if (estado_buzzer == BUZZER_IDLE) return;

    if (time_reached(tempo_buzzer)) {
        switch (estado_buzzer) {
            case BUZZER_BEEP_CURTO:
                desligar_buzzer();
                if (eh_duplo_beep_flag) {
                    // Se a flag de beep duplo estiver ativa, inicia a pausa
                    estado_buzzer = BUZZER_PAUSA_DUPLO;
                    tempo_buzzer = make_timeout_time_ms(PAUSA_ENTRE_BEEPS);
                    eh_duplo_beep_flag = false; // Reseta a flag
                } else {
                    // Se não, foi um beep único
                    estado_buzzer = BUZZER_IDLE;
                }
                break;

            case BUZZER_PAUSA_DUPLO:
                // Pausa terminou, inicia o segundo beep
                estado_buzzer = BUZZER_SEGUNDO_BEEP;
                tempo_buzzer = make_timeout_time_ms(DURACAO_BEEP_CURTO);
                ligar_buzzer(FREQ_BEEP_CURTO);
                break;

            case BUZZER_SEGUNDO_BEEP:
                // Segundo beep terminou
                desligar_buzzer();
                estado_buzzer = BUZZER_IDLE;
                break;

            case BUZZER_BEEP_LONGO:
                // Beep longo terminou
                desligar_buzzer();
                estado_buzzer = BUZZER_IDLE;
                break;

            case BUZZER_BEEP_PRONTO:
                // Beep de sistema pronto terminou
                desligar_buzzer();
                estado_buzzer = BUZZER_IDLE;
                break;

            default:
                estado_buzzer = BUZZER_IDLE;
                desligar_buzzer();
                break;
        }
    }
}

// FUNÇÕES DO LED RGB - Indicam o estado do sistema

// Configura os pinos do LED RGB como saídas
static void configurar_led_rgb(void) {
    gpio_init(LED_VERMELHO);
    gpio_init(LED_VERDE);
    gpio_init(LED_AZUL);

    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
}

// Liga/desliga as cores do LED RGB
static void definir_cor_led(bool vermelho, bool verde, bool azul) {
    gpio_put(LED_VERMELHO, vermelho);
    gpio_put(LED_VERDE, verde);
    gpio_put(LED_AZUL, azul);
}

// Pisca LED roxo quando há erro crítico (trava o sistema)
static void piscar_led_erro_critico(void) {
    // Emite beep longo de erro antes de travar
    iniciar_beep_longo();

    // Aguarda o beep terminar antes de travar
    while (estado_buzzer != BUZZER_IDLE) {
        atualizar_buzzer();
        sleep_ms(10);
    }

    while (1) {
        definir_cor_led(true, false, true);  // Roxo = erro
        sleep_ms(250);
        definir_cor_led(false, false, false); // Apagado
        sleep_ms(250);
    }
}

// FUNÇÕES DO DISPLAY OLED - Interface visual do usuário

// Configura e inicializa o display OLED
static void configurar_display_oled(void) {
    // Configura comunicação I²C para o display
    i2c_init(I2C_DISPLAY_PORTA, 400 * 1000);
    gpio_set_function(I2C_DISPLAY_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_DISPLAY_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_DISPLAY_SDA);
    gpio_pull_up(I2C_DISPLAY_SCL);

    // Inicializa o display OLED 128x64 pixels
    ssd1306_init(&display_oled, 128, 64, false, ENDERECO_OLED, I2C_DISPLAY_PORTA);
    ssd1306_config(&display_oled);
}

// Mostra a tela principal com status do sistema
static void mostrar_tela_principal(void) {
    // Limpa toda a tela
    ssd1306_fill(&display_oled, false);

    // Título do sistema na parte superior
    ssd1306_draw_string(&display_oled, "MPU6050 LOGGER", 14, 1, false);

    // Desenha linhas horizontais para separar seções
    ssd1306_hline(&display_oled, 0, 127, 12, true);
    ssd1306_hline(&display_oled, 0, 127, 30, true);
    ssd1306_hline(&display_oled, 0, 127, 48, true);

    // Mostra o status atual do sistema
    char buffer_status[30];
    snprintf(buffer_status, sizeof(buffer_status), "STATUS:%s", texto_status);
    ssd1306_draw_string(&display_oled, buffer_status, 0, 16, false);

    // Mostra quantas amostras foram coletadas
    char buffer_amostras[30];
    snprintf(buffer_amostras, sizeof(buffer_amostras), "AMOSTRAS: %lu", numero_amostras_display);
    ssd1306_draw_string(&display_oled, buffer_amostras, 0, 34, false);

    // Mostra mensagens adicionais na parte inferior
    ssd1306_draw_string(&display_oled, texto_mensagem, 0, 52, false);

    // Envia tudo para o display físico
    ssd1306_send_data(&display_oled);
}

// Mostra a tela com os valores atuais dos sensores
static void mostrar_tela_valores_sensores(void) {
    // Limpa toda a tela
    ssd1306_fill(&display_oled, false);

    // Título centralizado
    ssd1306_draw_string(&display_oled, "VALORES", 46, 1, false);

    // Valores dos sensores com maior espaçamento vertical
    char linha[30];
    int y = 10;  // Posição inicial vertical

    snprintf(linha, sizeof(linha), "ax: %.2f", dados_sensor_atuais.accel_x);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);
    y += 9;

    snprintf(linha, sizeof(linha), "ay: %.2f", dados_sensor_atuais.accel_y);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);
    y += 9;

    snprintf(linha, sizeof(linha), "az: %.2f", dados_sensor_atuais.accel_z);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);
    y += 9;

    snprintf(linha, sizeof(linha), "gx: %.2f", dados_sensor_atuais.gyro_x);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);
    y += 9;

    snprintf(linha, sizeof(linha), "gy: %.2f", dados_sensor_atuais.gyro_y);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);
    y += 9;

    snprintf(linha, sizeof(linha), "gz: %.2f", dados_sensor_atuais.gyro_z);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);

    // Envia tudo para o display físico
    ssd1306_send_data(&display_oled);
}

// Função auxiliar para converter valores de aceleração para pixels (barras horizontais)
static int normalizar_aceleracao_para_pixels_horizontal(float valor_aceleracao) {
    // Normaliza valores de -10g a +10g para largura de 0 a 60 pixels
    const float ACCEL_MAX = 10.0f;  // Máximo esperado em g
    const int LARGURA_MAXIMA_BARRA = 60;  // Largura máxima da barra em pixels

    // Limita o valor entre -ACCEL_MAX e +ACCEL_MAX
    if (valor_aceleracao > ACCEL_MAX) valor_aceleracao = ACCEL_MAX;
    if (valor_aceleracao < -ACCEL_MAX) valor_aceleracao = -ACCEL_MAX;

    // Converte para pixels mantendo o sinal (positivo ou negativo)
    float normalizado = valor_aceleracao / ACCEL_MAX;
    return (int)(normalizado * LARGURA_MAXIMA_BARRA);
}

// Mostra a tela com gráfico de barras horizontais das acelerações
static void mostrar_tela_grafico_aceleracao(void) {
    // Limpa toda a tela
    ssd1306_fill(&display_oled, false);

    // Título centralizado
    ssd1306_draw_string(&display_oled, "GRAFICO", 44, 1, false);

    // Linha separadora
    ssd1306_hline(&display_oled, 0, 127, 12, true);

    // Parâmetros das barras horizontais
    const int ALTURA_BARRA = 6;  // Altura de cada barra
    const int ESPACO_ENTRE_BARRAS = 4;  // Espaço entre barras
    const int CENTRO_X = 64;  // Centro da tela (ponto zero)
    const int LARGURA_MAXIMA = 60;  // Largura máxima de cada lado do centro

    // Posições Y das barras
    int y_ax = 20;  // Posição Y da barra de Accel_X
    int y_ay = y_ax + ALTURA_BARRA + ESPACO_ENTRE_BARRAS;  // Accel_Y
    int y_az = y_ay + ALTURA_BARRA + ESPACO_ENTRE_BARRAS;  // Accel_Z

    // Calcula largura das barras baseado na aceleração (pode ser negativa)
    int largura_ax = normalizar_aceleracao_para_pixels_horizontal(dados_sensor_atuais.accel_x);
    int largura_ay = normalizar_aceleracao_para_pixels_horizontal(dados_sensor_atuais.accel_y);
    int largura_az = normalizar_aceleracao_para_pixels_horizontal(dados_sensor_atuais.accel_z);

    // Desenha linha vertical central (referência zero)
    ssd1306_vline(&display_oled, CENTRO_X, 15, 55, true);

    // Desenha as barras horizontais
    // Barra Accel_X
    if (largura_ax != 0) {
        int inicio_x = (largura_ax > 0) ? CENTRO_X : CENTRO_X + largura_ax;
        int fim_x = (largura_ax > 0) ? CENTRO_X + largura_ax : CENTRO_X;
        for (int i = 0; i < ALTURA_BARRA; i++) {
            ssd1306_hline(&display_oled, inicio_x, fim_x, y_ax + i, true);
        }
    }

    // Barra Accel_Y
    if (largura_ay != 0) {
        int inicio_x = (largura_ay > 0) ? CENTRO_X : CENTRO_X + largura_ay;
        int fim_x = (largura_ay > 0) ? CENTRO_X + largura_ay : CENTRO_X;
        for (int i = 0; i < ALTURA_BARRA; i++) {
            ssd1306_hline(&display_oled, inicio_x, fim_x, y_ay + i, true);
        }
    }

    // Barra Accel_Z
    if (largura_az != 0) {
        int inicio_x = (largura_az > 0) ? CENTRO_X : CENTRO_X + largura_az;
        int fim_x = (largura_az > 0) ? CENTRO_X + largura_az : CENTRO_X;
        for (int i = 0; i < ALTURA_BARRA; i++) {
            ssd1306_hline(&display_oled, inicio_x, fim_x, y_az + i, true);
        }
    }

    // Labels das barras (à esquerda)
    ssd1306_draw_string(&display_oled, "X", 2, y_ax, false);
    ssd1306_draw_string(&display_oled, "Y", 2, y_ay, false);
    ssd1306_draw_string(&display_oled, "Z", 2, y_az, false);

    // Marcadores de escala (-10, 0, +10)
    ssd1306_draw_string(&display_oled, "-10", 0, 56, false);
    ssd1306_draw_string(&display_oled, "0", 61, 56, false);
    ssd1306_draw_string(&display_oled, "+10", 110, 56, false);

    // Envia tudo para o display físico
    ssd1306_send_data(&display_oled);
}

// Atualiza a tela do display baseado no estado atual
static void atualizar_tela(void) {
    switch (tela_atual) {
        case TELA_PRINCIPAL:
            mostrar_tela_principal();
            break;
        case TELA_VALORES:
            mostrar_tela_valores_sensores();
            break;
        case TELA_GRAFICO:
            mostrar_tela_grafico_aceleracao();
            break;
        default:
            mostrar_tela_principal();
            break;
    }
}

// Funções auxiliares para atualizar informações na tela
static void alterar_status_display(const char *novo_status) {
    strncpy(texto_status, novo_status, sizeof(texto_status) - 1);
    if (tela_atual == TELA_PRINCIPAL) {
        atualizar_tela();
    }
}

static void alterar_mensagem_display(const char *nova_mensagem) {
    strncpy(texto_mensagem, nova_mensagem, sizeof(texto_mensagem) - 1);
    if (tela_atual == TELA_PRINCIPAL) {
        atualizar_tela();
    }
}

static void alterar_contador_amostras_display(uint32_t numero) {
    numero_amostras_display = numero;
    if (tela_atual == TELA_PRINCIPAL) {
        atualizar_tela();
    }
}

// Cicla entre as telas: principal -> valores -> gráfico -> principal
static void ciclar_telas(void) {
    tela_atual = (tela_atual + 1) % TOTAL_TELAS;

    if (tela_atual != TELA_PRINCIPAL) {
        // Lê dados atuais do sensor para exibição
        mpu6050_read_data(&dados_sensor_atuais);
        // Agenda primeira atualização
        proxima_atualizacao_valores = get_absolute_time();
    }

    atualizar_tela();
}

// FUNÇÕES DO CARTÃO SD - Gerenciam armazenamento de dados

// Busca um cartão SD específico pelo nome
static sd_card_t *buscar_cartao_sd_por_nome(const char *nome) {
    for (size_t i = 0; i < sd_get_num(); ++i) {
        if (!strcmp(sd_get_by_num(i)->pcName, nome)) {
            return sd_get_by_num(i);
        }
    }
    return NULL;
}

// Busca sistema de arquivos do cartão SD pelo nome
static FATFS *buscar_sistema_arquivos_por_nome(const char *nome) {
    for (size_t i = 0; i < sd_get_num(); ++i) {
        if (!strcmp(sd_get_by_num(i)->pcName, nome)) {
            return &sd_get_by_num(i)->fatfs;
        }
    }
    return NULL;
}

// Conecta e monta o cartão SD para uso
static bool conectar_cartao_sd(void) {
    if (cartao_sd_conectado) return true;

    const char *nome_drive = sd_get_by_num(0)->pcName;
    FATFS *sistema_arquivos = buscar_sistema_arquivos_por_nome(nome_drive);

    if (!sistema_arquivos) {
        printf("Drive do cartão SD não encontrado.\n");
        return false;
    }

    // Tenta montar o cartão SD
    FRESULT resultado = f_mount(sistema_arquivos, nome_drive, 1);
    if (resultado != FR_OK) {
        printf("Erro ao montar cartão SD: %s\n", FRESULT_str(resultado));
        return false;
    }

    buscar_cartao_sd_por_nome(nome_drive)->mounted = true;
    cartao_sd_conectado = true;
    printf("Cartão SD conectado com sucesso.\n");
    return true;
}

// Desconecta o cartão SD de forma segura
static void desconectar_cartao_sd(void) {
    if (!cartao_sd_conectado) return;

    // Para a gravação se estiver ativa
    if (esta_gravando) {
        esta_gravando = false;
        definir_cor_led(false, true, false); // LED verde = parado
    }

    const char *nome_drive = sd_get_by_num(0)->pcName;
    f_unmount(nome_drive);
    buscar_cartao_sd_por_nome(nome_drive)->mounted = false;
    cartao_sd_conectado = false;

    definir_cor_led(false, false, false); // LED apagado
    alterar_status_display("SD OFF");
    alterar_mensagem_display("");
    printf("Cartão SD desconectado.\n");
}

// FUNÇÕES DE GRAVAÇÃO DE DADOS

// Cria o arquivo CSV com o cabeçalho das colunas
static void criar_arquivo_csv_com_cabecalho(void) {
    if (!cartao_sd_conectado) return;

    FIL arquivo;
    if (f_open(&arquivo, "dados_MPU3.csv", FA_WRITE | FA_CREATE_NEW) == FR_OK) {
        const char *cabecalho = 
            "Amostra,Acel_X,Acel_Y,Acel_Z,Giro_X,Giro_Y,Giro_Z,Temperatura\n";
        f_write(&arquivo, cabecalho, strlen(cabecalho), NULL);
        f_close(&arquivo);
        printf("Arquivo CSV criado com sucesso.\n");
    }
}

// Lê dados do sensor MPU6050 e salva no cartão SD
static void gravar_dados_do_sensor(void) {
    if (!cartao_sd_conectado) {
        alterar_status_display("ERRO: SEM SD");
        piscar_led_erro_critico();
    }

    // LED azul indica que está gravando dados
    definir_cor_led(false, false, true);

    // Abre o arquivo CSV para adicionar nova linha
    FIL arquivo;
    if (f_open(&arquivo, "dados_MPU3.csv", FA_WRITE | FA_OPEN_APPEND) != FR_OK) {
        alterar_status_display("ERRO ARQUIVO");
        piscar_led_erro_critico();
    }

    // Lê os dados atuais do sensor MPU6050
    mpu6050_read_data(&dados_sensor_atuais);

    // Formata os dados em uma linha CSV
    char linha_dados[256];
    snprintf(linha_dados, sizeof(linha_dados),
        "%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f\n",
        ++contador_amostras,
        dados_sensor_atuais.accel_x, dados_sensor_atuais.accel_y, dados_sensor_atuais.accel_z,
        dados_sensor_atuais.gyro_x,  dados_sensor_atuais.gyro_y,  dados_sensor_atuais.gyro_z,
        dados_sensor_atuais.temp_c);

    // Grava a linha no arquivo e fecha
    f_write(&arquivo, linha_dados, strlen(linha_dados), NULL);
    f_close(&arquivo);

    // LED vermelho indica sistema ativo
    definir_cor_led(true, false, false);

    // Atualiza informações na tela
    alterar_contador_amostras_display(contador_amostras);
    alterar_mensagem_display("Dados salvos");
}

// FUNÇÕES DE CONTROLE DA GRAVAÇÃO

// Inicia o processo de coleta e gravação de dados
static void iniciar_gravacao_dados(void) {
    if (!cartao_sd_conectado) {
        printf("Cartão SD não está conectado.\n");
        return;
    }
    if (esta_gravando) return; // Já está gravando

    esta_gravando = true;
    definir_cor_led(true, false, false); // LED vermelho = gravando
    alterar_status_display("GRAVANDO");
    alterar_mensagem_display("");
    proxima_medicao = get_absolute_time();

    // Emite beep curto ao iniciar a coleta (não-bloqueante)
    iniciar_beep_curto();
}

// Para o processo de coleta e gravação de dados
static void parar_gravacao_dados(void) {
    if (!esta_gravando) return; // Já está parado

    esta_gravando = false;
    definir_cor_led(false, true, false); // LED verde = parado
    alterar_status_display("PAUSADO");
    alterar_mensagem_display("");

    // Emite dois beeps curtos ao parar a coleta (não-bloqueante)
    iniciar_dois_beeps();
}

// FUNÇÕES DOS BOTÕES DE CONTROLE

// Função chamada quando um botão é pressionado
static void processar_clique_botao(uint pino_gpio, uint32_t eventos) {
    // Implementa debounce - evita múltiplos cliques acidentais
    static uint64_t ultimo_clique = 0;
    uint64_t agora = time_us_64();
    if (agora - ultimo_clique < TEMPO_DEBOUNCE_US) return;
    ultimo_clique = agora;

    // Processa ação baseada no botão pressionado
    if (pino_gpio == BOTAO_CARTAO_SD) {
        // Alterna entre conectar/desconectar cartão SD
        if (cartao_sd_conectado) {
            desconectar_cartao_sd();
        } else {
            conectar_cartao_sd();
        }
    } else if (pino_gpio == BOTAO_GRAVACAO) {
        // Alterna entre iniciar/parar gravação
        if (esta_gravando) {
            parar_gravacao_dados();
        } else {
            iniciar_gravacao_dados();
        }
    } else if (pino_gpio == BOTAO_VALORES) {
        // Cicla entre as telas: principal -> valores -> gráfico -> principal
        ciclar_telas();
    }
}

// Configura os botões e suas interrupções
static void configurar_botoes_controle(void) {
    // Configura pinos dos botões como entrada com pull-up interno
    gpio_init(BOTAO_CARTAO_SD);
    gpio_init(BOTAO_GRAVACAO);
    gpio_init(BOTAO_VALORES);
    gpio_set_dir(BOTAO_CARTAO_SD, GPIO_IN);
    gpio_set_dir(BOTAO_GRAVACAO, GPIO_IN);
    gpio_set_dir(BOTAO_VALORES, GPIO_IN);
    gpio_pull_up(BOTAO_CARTAO_SD);
    gpio_pull_up(BOTAO_GRAVACAO);
    gpio_pull_up(BOTAO_VALORES);

    // Configura interrupções para detectar quando botões são pressionados
    gpio_set_irq_enabled_with_callback(BOTAO_CARTAO_SD, GPIO_IRQ_EDGE_FALL, true, &processar_clique_botao);
    gpio_set_irq_enabled_with_callback(BOTAO_GRAVACAO, GPIO_IRQ_EDGE_FALL, true, &processar_clique_botao);
    gpio_set_irq_enabled_with_callback(BOTAO_VALORES, GPIO_IRQ_EDGE_FALL, true, &processar_clique_botao);
}

// FUNÇÃO DE INICIALIZAÇÃO DO SISTEMA

// Inicializa todos os componentes do sistema
static bool inicializar_sistema_completo(void) {
    alterar_status_display("INICIANDO...");
    definir_cor_led(true, true, false); // LED amarelo = inicializando

    // Inicializa driver do cartão SD
    if (!sd_init_driver()) {
        printf("Erro ao inicializar driver do cartão SD.\n");
        return false;
    }

    // Conecta cartão SD
    if (!conectar_cartao_sd()) {
        printf("Erro ao conectar cartão SD.\n");
        return false;
    }

    // Configura comunicação I²C para o sensor MPU6050
    i2c_init(I2C_SENSOR_PORTA, 400 * 1000);
    gpio_set_function(I2C_SENSOR_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SENSOR_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SENSOR_SDA);
    gpio_pull_up(I2C_SENSOR_SCL);

    // Inicializa o sensor MPU6050
    mpu6050_init(I2C_SENSOR_PORTA);

    // Configura botões de controle
    configurar_botoes_controle();

    // Cria arquivo CSV inicial
    criar_arquivo_csv_com_cabecalho();

    // Sistema pronto para uso
    definir_cor_led(false, true, false); // LED verde = pronto
    alterar_status_display("PRONTO");
    iniciar_beep_pronto(); // Emite beep para indicar que o sistema está pronto
    return true;
}

// FUNÇÃO PRINCIPAL DO PROGRAMA

int main(void) {
    // Inicializa comunicação USB para debug
    stdio_init_all();

    // Inicializa componentes básicos
    configurar_led_rgb();
    configurar_buzzer();  // Inicializa o buzzer
    configurar_display_oled();
    atualizar_tela(); // Mostra tela inicial

    // Aguarda um tempo para estabilizar o sistema
    sleep_ms(2500);

    // Inicializa todo o sistema
    if (!inicializar_sistema_completo()) {
        alterar_status_display("ERRO FATAL");
        piscar_led_erro_critico(); // Trava aqui se houver erro (com beep longo)
    }

    // Inicializa tempo para primeira atualização de valores
    proxima_atualizacao_valores = get_absolute_time();

    // Loop principal do programa
    while (1) {
        // ATUALIZA O BUZZER PRIMEIRO (não-bloqueante)
        atualizar_buzzer();

        // Se está na tela de valores ou gráfico, atualiza os dados periodicamente
        if ((tela_atual == TELA_VALORES || tela_atual == TELA_GRAFICO) &&
            time_reached(proxima_atualizacao_valores)) {
            // Lê novos dados do sensor
            mpu6050_read_data(&dados_sensor_atuais);
            // Atualiza a tela com os novos valores
            atualizar_tela();
            // Agenda próxima atualização
            proxima_atualizacao_valores = make_timeout_time_ms(TEMPO_ATUALIZACAO_VALORES_MS);
        }

        // Se está gravando E cartão SD conectado E chegou a hora da próxima medição
        if (esta_gravando && cartao_sd_conectado && time_reached(proxima_medicao)) {
            // Agenda próxima medição
            proxima_medicao = make_timeout_time_ms(TEMPO_ENTRE_LEITURAS_MS);
            // Grava dados do sensor
            gravar_dados_do_sensor();
        }

        // Pequena pausa para não sobrecarregar o processador
        sleep_ms(5);
    }
}
