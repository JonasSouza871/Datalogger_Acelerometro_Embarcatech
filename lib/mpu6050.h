#ifndef MPU6050_H
#define MPU6050_H

#include "hardware/i2c.h"

// Estrutura para armazenar os dados lidos do sensor já convertidos
typedef struct {
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    float temp_c;
} mpu6050_data_t;

// Inicializa o sensor MPU6050, configurando-o e tirando-o do modo de suspensão
void mpu6050_init(i2c_inst_t *i2c);

// Lê os dados brutos do MPU6050, converte para unidades padrão e preenche a estrutura fornecida
void mpu6050_read_data(mpu6050_data_t *data);

#endif // MPU6050_H