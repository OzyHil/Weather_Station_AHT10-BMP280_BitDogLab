#include "General.h" // Inclusão da biblioteca geral do sistema

// Função para inicializar a configuração do sistema
void init_system_config()
{

    adc_init(); // Inicializa o conversor analógico-digital (ADC)
    
    // Configura o relógio do sistema para 125 kHz (sistema de 32 bits com precisão de tempo)
    set_sys_clock_khz(125000, false);

    // Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();

        // Inicializa o barramento I2C na porta e com frequência de 400 kHz
    i2c_init(I2C_PORT_0, 400 * 1000);

    // Define as funções dos pinos SDA e SCL como I2C
    gpio_set_function(I2C_SDA_0, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_0, GPIO_FUNC_I2C);

    // Ativa resistores de pull-up nos pinos SDA e SCL
    gpio_pull_up(I2C_SDA_0);
    gpio_pull_up(I2C_SCL_0);

        // Inicializa o barramento I2C na porta e com frequência de 400 kHz
    i2c_init(I2C_PORT_1, 400 * 1000);

    // Define as funções dos pinos SDA e SCL como I2C
    gpio_set_function(I2C_SDA_1, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_1, GPIO_FUNC_I2C);

    // Ativa resistores de pull-up nos pinos SDA e SCL
    gpio_pull_up(I2C_SDA_1);
    gpio_pull_up(I2C_SCL_1);
}

// Função para inicializar a funcionalidade PWM em um pino GPIO
void init_pwm(uint gpio, uint wrap)
{
    gpio_set_function(gpio, GPIO_FUNC_PWM); // Define o pino GPIO para a função PWM

    uint slice = pwm_gpio_to_slice_num(gpio); // Obtém o número do slice PWM associado ao pino GPIO

    pwm_set_clkdiv(slice, 16.0); // Define o divisor de clock PWM (controla a velocidade do sinal PWM)

    pwm_set_wrap(slice, wrap); // Define o valor de "wrap", que determina o ciclo completo do PWM

    pwm_set_enabled(slice, true); // Habilita a geração do sinal PWM no slice
}

// Adiciona um novo valor ao histórico de leituras, removendo o mais antigo se necessário
void add_reading(int32_t new_value, int32_t readings[])
{
    for (int i = 0; i < MAX_READINGS - 1; i++)
    {
        readings[i] = readings[i + 1];
    }
    readings[MAX_READINGS - 1] = new_value;
}