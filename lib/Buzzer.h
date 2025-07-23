#ifndef BUZZER_H
#define BUZZER_H

// Inclusão da biblioteca geral do sistema
#include "General.h"

// Definição do pino e valor de PWM para o buzzer
#define BUZZER_A 21          // Pino do buzzer
#define WRAP_PWM_BUZZER 3000 // Valor de wrap para PWM do buzzer

// Função para configurar o buzzer
void configure_buzzer();

// Função para definir o nível do buzzer (intensidade do som)
void set_buzzer_level(uint gpio, uint16_t level);

void double_beep(); // Função para emitir dois bipes

void single_beep(); // Função para emitir um único bip

#endif