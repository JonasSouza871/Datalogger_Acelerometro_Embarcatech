#include "mpu6050.h"
#include "pico/stdlib.h"
#include <stdio.h>

// Endereço I2C padrão do MPU6050
static const uint8_t MPU6050_ADDR = 0x68;

// Registradores do MPU6050
static const uint8_t REG_PWR_MGMT_1 = 0x6B;
static const uint8_t REG_ACCEL_XOUT_H = 0x3B;
static const uint8_t REG_GYRO_XOUT_H = 0x43;
static const uint8_t REG_TEMP_OUT_H = 0x41;

// Fatores de sensibilidade (para a configuração padrão)
// Aceleração: ±2g -> 16384 LSB/g
// Giroscópio: ±250°/s -> 131 LSB/°/s
// A aceleração da gravidade (g) é ~9.81 m/s²
static const float ACCEL_SENSITIVITY = 16384.0;
static const float GYRO_SENSITIVITY = 131.0;
static const float GRAVITY_MS2 = 9.81;

// Ponteiro para a instância I2C usada
static i2c_inst_t *i2c_port;

/**
 * @brief Reseta o MPU6050 e o tira do modo de suspensão.
 * Função interna chamada por mpu6050_init.
 */
static void mpu6050_reset() {
    uint8_t buf[] = {REG_PWR_MGMT_1, 0x80};
    i2c_write_blocking(i2c_port, MPU6050_ADDR, buf, 2, false);
    sleep_ms(100); // Aguarda o reset

    buf[1] = 0x00; // Acorda o dispositivo
    i2c_write_blocking(i2c_port, MPU6050_ADDR, buf, 2, false);
    sleep_ms(10); // Aguarda estabilização
}

// Implementação da função de inicialização
void mpu6050_init(i2c_inst_t *i2c) {
    i2c_port = i2c;
    mpu6050_reset();
    printf("MPU6050 inicializado com sucesso.\n");
}

// Implementação da função de leitura e conversão de dados
void mpu6050_read_data(mpu6050_data_t *data) {
    uint8_t buffer[14];
    
    // Inicia a leitura a partir do registrador de aceleração (0x3B)
    // O MPU6050 auto-incrementa o endereço, então podemos ler tudo de uma vez
    uint8_t start_reg = REG_ACCEL_XOUT_H;
    i2c_write_blocking(i2c_port, MPU6050_ADDR, &start_reg, 1, true); // true para manter o controle do barramento
    i2c_read_blocking(i2c_port, MPU6050_ADDR, buffer, 14, false);

    // 1. Extrai e combina os bytes para formar os valores brutos (int16_t)
    int16_t raw_ax = (buffer[0] << 8) | buffer[1];
    int16_t raw_ay = (buffer[2] << 8) | buffer[3];
    int16_t raw_az = (buffer[4] << 8) | buffer[5];
    int16_t raw_temp = (buffer[6] << 8) | buffer[7];
    int16_t raw_gx = (buffer[8] << 8) | buffer[9];
    int16_t raw_gy = (buffer[10] << 8) | buffer[11];
    int16_t raw_gz = (buffer[12] << 8) | buffer[13];

    // 2. Converte os valores brutos para unidades físicas
    // Aceleração: LSB -> g -> m/s²
    data->accel_x = (raw_ax / ACCEL_SENSITIVITY) * GRAVITY_MS2;
    data->accel_y = (raw_ay / ACCEL_SENSITIVITY) * GRAVITY_MS2;
    data->accel_z = (raw_az / ACCEL_SENSITIVITY) * GRAVITY_MS2;

    // Giroscópio: LSB -> °/s
    data->gyro_x = raw_gx / GYRO_SENSITIVITY;
    data->gyro_y = raw_gy / GYRO_SENSITIVITY;
    data->gyro_z = raw_gz / GYRO_SENSITIVITY;

   // Temperatura: usa a fórmula do datasheet com correção de calibração
    data->temp_c = (raw_temp / 340.0) + 36.53 - 24.0;
}
