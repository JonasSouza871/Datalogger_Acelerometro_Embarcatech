#include "matriz_led.h"
#include <stdlib.h>

/* ---------- Padrões (bitmaps 5x5) ---------- */
// Padrão "!" (Alerta) para Vermelho / Amarelo
const uint8_t PAD_EXC[5] = {0b00100, 0b00100, 0b00100, 0b00000, 0b00100};
// Padrão "X" (Erro/Aviso) para Cinza
const uint8_t PAD_X[5]   = {0b10001, 0b01010, 0b00100, 0b01010, 0b10001};
// Padrão "Quadrado" (3x3 centralizado) para Branco e Verde
const uint8_t PAD_QUADRADO[5] = {0b00000, 0b01110, 0b01110, 0b01110, 0b00000};

/* ---------- Funções Internas (static) ---------- */

// Envia um valor de cor 24 bits (formato GRB) para a state machine do PIO
static inline void ws2812_put(uint32_t grb) {
    pio_sm_put_blocking(pio0, 0, grb << 8u);
}

// Desenha um padrão estático na matriz
static void matriz_draw_pattern(const uint8_t pad[5], uint32_t cor_on) {
    // A matriz física foi montada de cabeça para baixo, então iteramos da linha 4 para a 0
    for (int lin = 4; lin >= 0; --lin) {
        for (int col = 0; col < 5; ++col) {
            // Verifica se o bit correspondente no padrão está aceso
            bool aceso = pad[lin] & (1 << (4 - col));
            ws2812_put(aceso ? cor_on : COR_OFF);
        }
    }
    sleep_us(60); // Pequena pausa para garantir atualização do hardware
}

// Desenha animação de gotas de chuva caindo
// Mantém estado das gotas entre chamadas usando variáveis estáticas
static void matriz_draw_rain_animation(uint32_t cor_on) {
    static uint8_t gotas_y[NUM_COLUNAS] = {0}; // Posição Y (0-5, 0=desligada) de cada gota
    static uint32_t ultimo_tempo = 0;
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());

    // Controla velocidade da animação (framerate)
    if (tempo_atual - ultimo_tempo < 150) {
        return; // Sai se não for hora de atualizar o quadro
    }
    ultimo_tempo = tempo_atual;

    uint32_t buffer_pixels[NUM_PIXELS];

    // 1. Limpa buffer de pixels (cor de fundo)
    for (int i = 0; i < NUM_PIXELS; i++) {
        buffer_pixels[i] = COR_OFF;
    }

    // 2. "Desenha" gotas no buffer
    for (int col = 0; col < NUM_COLUNAS; col++) {
        if (gotas_y[col] > 0) {
            int lin = gotas_y[col] - 1;
            // Mapeia para linha física (montagem invertida)
            int physical_lin = (NUM_LINHAS - 1) - lin;
            int pixel_index = physical_lin * NUM_COLUNAS + col;
            if (pixel_index >= 0 && pixel_index < NUM_PIXELS) {
                buffer_pixels[pixel_index] = cor_on;
            }
        }
    }

    // 3. Atualiza posição das gotas para próximo quadro
    for (int col = 0; col < NUM_COLUNAS; col++) {
        if (gotas_y[col] > 0) {
            gotas_y[col]++; // Move a gota para baixo
            if (gotas_y[col] > NUM_LINHAS) {
                gotas_y[col] = 0; // Gota some ao atingir final
            }
        } else {
            // Chance de criar uma nova gota no topo (20% por quadro)
            if (rand() % 10 < 2) {
                gotas_y[col] = 1;
            }
        }
    }

    // 4. Envia buffer de pixels completo para matriz
    for (int i = 0; i < NUM_PIXELS; i++) {
        ws2812_put(buffer_pixels[i]);
    }
    sleep_us(60);
}

/* ---------- Funções da API Pública ---------- */

void inicializar_matriz_led(void) {
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, 0, offset, PINO_WS2812, 800000, RGBW_ATIVO);
    // Inicializa gerador de números aleatórios para animação de chuva
    srand(to_us_since_boot(get_absolute_time()));
}

void matriz_clear(void) {
    for (int i = 0; i < NUM_PIXELS; ++i)
        ws2812_put(COR_OFF);
    sleep_us(60);
}

void atualizar_matriz_pelo_estado(EstadoSistema estado) {
    static EstadoSistema estado_anterior = -1; // Estado inválido para forçar primeira atualização

    switch (estado) {
        // --- ESTADOS COM ANIMAÇÃO CONTÍNUA ---
        // Estes casos devem ser chamados em cada loop para animação funcionar
        case ESTADO_TEMP_BAIXA:
            matriz_draw_rain_animation(COR_AZUL);
            break;
        case ESTADO_UMID_ALTA:
            matriz_draw_rain_animation(COR_VIOLETA);
            break;

        // --- ESTADOS COM PADRÕES ESTÁTICOS ---
        // O padrão só é redesenhado se o estado mudou
        default:
            if (estado != estado_anterior) {
                switch (estado) {
                    case ESTADO_NORMAL:
                        matriz_draw_pattern(PAD_QUADRADO, COR_VERDE);
                        break;
                    case ESTADO_TEMP_ALTA:
                        matriz_draw_pattern(PAD_EXC, COR_VERMELHO);
                        break;
                    case ESTADO_UMID_BAIXA:
                        matriz_draw_pattern(PAD_EXC, COR_AMARELO);
                        break;
                    case ESTADO_PRESS_ALTA:
                        matriz_draw_pattern(PAD_QUADRADO, COR_BRANCO);
                        break;
                    case ESTADO_PRESS_BAIXA:
                        matriz_draw_pattern(PAD_X, COR_CINZA);
                        break;
                    default:
                        matriz_clear();
                        break;
                }
            }
            break;
    }
    // Armazena estado atual para próxima verificação
    estado_anterior = estado;
}
