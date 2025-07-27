#ifndef MATRIZ_LED_H
#define MATRIZ_LED_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/ws2812.pio.h"

/* ---------- Configurações da Matriz ---------- */
#define PINO_WS2812   7     // Pino GPIO para comunicação com WS2812
#define NUM_LINHAS    5     // Número de linhas da matriz
#define NUM_COLUNAS   5     // Número de colunas da matriz
#define NUM_PIXELS    (NUM_LINHAS * NUM_COLUNAS)  // Total de LEDs (25)
#define RGBW_ATIVO    false // Define protocolo RGB (não RGBW)

/* ---------- Utilidades de Cor ---------- */
// Converte RGB para formato GRB do WS2812
#define GRB(r,g,b)    ( ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | (b) )

/* ---------- Cores Básicas para Padrões ---------- */
#define COR_BRANCO    GRB(255, 255, 255)
#define COR_PRATA     GRB(192, 192, 192)
#define COR_CINZA     GRB(40, 35, 35)    // Cor para pressão baixa
#define COR_VIOLETA   GRB(130, 0, 130)
#define COR_AZUL      GRB(0, 0, 200)
#define COR_MARROM    GRB(30, 10, 10)
#define COR_VERDE     GRB(0, 150, 0)
#define COR_OURO      GRB(218, 165, 32)
#define COR_LARANJA   GRB(255, 65, 0)
#define COR_AMARELO   GRB(255, 140, 0)
#define COR_VERMELHO  GRB(190, 0, 0)
#define COR_OFF       GRB(0, 0, 0)

/* ---------- Padrões 5x5 (✓, !, X) ---------- */
extern const uint8_t PAD_OK[5];   // Padrão "✓" para OK
extern const uint8_t PAD_EXC[5];  // Padrão "!" para Alerta
extern const uint8_t PAD_X[5];    // Padrão "X" para Erro/Crítico

/* ---------- Estados do Sistema ---------- */
// Enumeração dos possíveis estados do sistema para determinar cor e animação
typedef enum {
    ESTADO_NORMAL,
    ESTADO_TEMP_ALTA,
    ESTADO_TEMP_BAIXA,
    ESTADO_UMID_ALTA,
    ESTADO_UMID_BAIXA,
    ESTADO_PRESS_ALTA,
    ESTADO_PRESS_BAIXA
} EstadoSistema;

/* ---------- API Principal da Biblioteca ---------- */

// Inicializa o PIO e o pino para comunicação com a matriz WS2812
void inicializar_matriz_led(void);

// Atualiza matriz com padrão/animação correspondente ao estado do sistema
// Deve ser chamada continuamente no loop principal para animações fluidas
void atualizar_matriz_pelo_estado(EstadoSistema estado);

// Limpa a matriz (desliga todos os LEDs)
void matriz_clear(void);

#endif /* MATRIZ_LED_H */
