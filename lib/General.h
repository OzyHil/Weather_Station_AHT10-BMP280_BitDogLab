#ifndef GENERAL_H
#define GENERAL_H

// Inclusão das bibliotecas padrão e específicas do hardware
#include <stdio.h>  // Biblioteca padrão para entrada/saída
#include <stdlib.h> // Biblioteca padrão para alocação de memória e conversões
#include <stdint.h> // Biblioteca padrão para tipos inteiros
#include <string.h> // Biblioteca manipular strings

#include <math.h> // Biblioteca para funções matemáticas

#include "pico/stdlib.h" // Biblioteca principal para o Raspberry Pi Pico

#include "hardware/gpio.h"   // Controle de GPIO (General Purpose Input/Output)
#include "hardware/pwm.h"    // Controle de PWM (Pulse Width Modulation)
#include "hardware/pio.h"    // Programação de E/S PIO (Programmable I/O)
#include "hardware/clocks.h" // Controle de clocks
#include "hardware/i2c.h"    // Comunicação I2C
#include "hardware/adc.h"    // Biblioteca da Raspberry Pi Pico para manipulação do conversor ADC

#include "pio_matrix.pio.h" // Programa específico para controle da matriz de LEDs

#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "lwip/pbuf.h"       // Lightweight IP stack - manipulação de buffers de pacotes de rede
#include "lwip/tcp.h"        // Lightweight IP stack - fornece funções e estruturas para trabalhar com o protocolo TCP
#include "lwip/netif.h"      // Lightweight IP stack - fornece funções e estruturas para trabalhar com interfaces de rede (netif)

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Definição de constantes e macros
#define MIN_TEMP_LEVEL -40    
#define MAX_TEMP_LEVEL 85    

#define MIN_HUM_LEVEL 0    
#define MAX_HUM_LEVEL 100

#define MIN_PRESS_LEVEL 300  
#define MAX_PRESS_LEVEL 1100   

#define MAX_READINGS 10 // Pino da bomba de água

extern int32_t g_temperature;
extern int32_t g_pressure;
extern int32_t g_humidity;

extern int32_t g_offset_humidity;
extern int32_t g_offset_temperature;
extern int32_t g_offset_pressure;

extern int32_t g_temperature_max_limit;
extern int32_t g_pressure_max_limit;
extern int32_t g_humidity_max_limit;

extern int32_t g_temperature_min_limit;
extern int32_t g_pressure_min_limit;
extern int32_t g_humidity_min_limit;

extern int32_t g_temperature_historic_levels[MAX_READINGS];
extern int32_t g_pressure_historic_levels[MAX_READINGS];
extern int32_t g_humidity_historic_levels[MAX_READINGS]; 

extern SemaphoreHandle_t xLevelsLimitsMutex, xReadingsMutex, xOffsetMutex;

#define I2C_PORT_0 i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA_0 0                   // 0 ou 2
#define I2C_SCL_0 1                   // 1 ou 3
#define SEA_LEVEL_PRESSURE 101325.0 // Pressão ao nível do mar em Pa

#define I2C_PORT_1 i2c1
#define I2C_SDA_1 2
#define I2C_SCL_1 3

// Enum para definir os estados do sistema
typedef enum
{
    SYSTEM_ALERT,
    SYSTEM_NORMAL,
} system_state_t;

// Função para inicializar a configuração do sistema (clocks, I/O, etc.)
void init_system_config();

// Função para inicializar o PWM em um pino específico com um valor de wrap
void init_pwm(uint gpio, uint wrap);

void add_reading(int32_t new_value, int32_t readings[]);

#endif