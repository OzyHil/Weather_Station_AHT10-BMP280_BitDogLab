#include "General.h"
#include "Led.h"
#include "Buzzer.h"
#include "Button.h"
#include "Display.h"
#include "Led_Matrix.h"
#include "Webserver.h"
#include "Potentiometer.h"
#include "aht20.h"
#include "bmp280.h"
#include "pico/bootrom.h"

struct bmp280_calib_param params;
system_state_t g_current_system_state = SYSTEM_NORMAL; // Variável global para armazenar o estado atual do sistema

int32_t g_temperature = 0;
int32_t g_pressure = 0;
int32_t g_humidity = 0;

int32_t g_offset_humidity = 0;
int32_t g_offset_temperature = 0;
int32_t g_offset_pressure = 0;

int32_t g_temperature_max_limit = MAX_TEMP_LEVEL;
int32_t g_temperature_min_limit = MIN_TEMP_LEVEL;

int32_t g_pressure_max_limit = MAX_PRESS_LEVEL;
int32_t g_pressure_min_limit = MIN_PRESS_LEVEL;

int32_t g_humidity_max_limit = MAX_HUM_LEVEL;
int32_t g_humidity_min_limit = MIN_HUM_LEVEL;

int32_t g_temperature_historic_levels[MAX_READINGS] = {0};
int32_t g_pressure_historic_levels[MAX_READINGS] = {0};
int32_t g_humidity_historic_levels[MAX_READINGS] = {0};

int32_t g_temperature;
int32_t g_pressure;
int32_t g_humidity;

// Estrutura para armazenar os dados do sensor
AHT20_Data data;
int32_t raw_temp_bmp;
int32_t raw_pressure;

// Semáforos para controle de acesso e sincronização
SemaphoreHandle_t xDisplayMode,
    xResetThresholds,
    xReadingsMutex,
    xLevelsLimitsMutex,
    xStateMutex,
    xOffsetMutex;

// Variáveis para controle de tempo de debounce dos botões
uint last_time_button_A, last_time_button_B, last_time_button_J = 0;

// Função para sinalizar uma tarefa a partir de uma ISR
void signal_task_from_isr(SemaphoreHandle_t xSemaphore)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;                // Variável para verificar se uma tarefa de maior prioridade foi despertada
    xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken); // Libera o semáforo e verifica se uma tarefa de maior prioridade foi despertada
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);                 // Se uma tarefa de maior prioridade foi despertada, realiza um yield para que ela possa ser executada imediatamente
}

// Função de tratamento de interrupção para os botões
void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t now = us_to_ms(get_absolute_time());

    if (gpio == BUTTON_A && (now - last_time_button_A > DEBOUNCE_TIME))
    {
        last_time_button_A = now;
        signal_task_from_isr(xDisplayMode);
    }
    if (gpio == BUTTON_B && (now - last_time_button_B > DEBOUNCE_TIME))
    {
        last_time_button_B = now;
        reset_usb_boot(0, 0);
    }
    else if (gpio == BUTTON_J && (now - last_time_button_J > DEBOUNCE_TIME))
    {
        last_time_button_J = now;
        signal_task_from_isr(xResetThresholds);
    }
}

void vTaskResetThresholds()
{
    while (1)
    {
        if (xSemaphoreTake(xResetThresholds, portMAX_DELAY) == pdTRUE) // Espera pelo sinal do botão J
        {
            if (xSemaphoreTake(xLevelsLimitsMutex, portMAX_DELAY) == pdTRUE) // Espera pelo sinal do botão J
            {
                g_offset_humidity = 0;
                g_offset_temperature = 0;
                g_offset_pressure = 0;

                g_temperature_max_limit = MAX_TEMP_LEVEL;
                g_temperature_min_limit = MIN_TEMP_LEVEL;

                g_pressure_max_limit = MAX_PRESS_LEVEL;
                g_pressure_min_limit = MIN_PRESS_LEVEL;

                g_humidity_max_limit = MAX_HUM_LEVEL;
                g_humidity_min_limit = MIN_HUM_LEVEL;
                xSemaphoreGive(xLevelsLimitsMutex);
            }
        }
    }
}

void vTaskLedMatrix()
{
    display_mode_t current_display_mode = DISPLAY_TEMP; // Modo de exibição atual

    while (1)
    {
        if (xSemaphoreTake(xDisplayMode, portMAX_DELAY))
        {
            if (current_display_mode == DISPLAY_TEMP) // Verifica o modo de exibição atual
            {
                current_display_mode = DISPLAY_HUM; // Muda para o próximo modo de exibição
            }
            else if (current_display_mode == DISPLAY_HUM)
            {
                current_display_mode = DISPLAY_PRESS; // Muda para o próximo modo de exibição
            }
            else if (current_display_mode == DISPLAY_PRESS)
            {
                current_display_mode = DISPLAY_TEMP; // Volta para o modo de exibição inicial
            }
            xSemaphoreGive(xDisplayMode); // Libera o mutex do display
        }

        if (xSemaphoreTake(xReadingsMutex, portMAX_DELAY) == pdTRUE)
        {
            if (current_display_mode == DISPLAY_TEMP) // Verifica o modo de exibição atual
            {
                update_matrix_from_level(g_temperature, MAX_TEMP_LEVEL); // Atualiza a matriz de LEDs com o nível de temperatura
            }
            else if (current_display_mode == DISPLAY_HUM)
            {
                update_matrix_from_level(g_humidity, MAX_HUM_LEVEL); // Atualiza a matriz de LEDs com o nível de umidade
            }
            else if (current_display_mode == DISPLAY_PRESS)
            {
                update_matrix_from_level(g_pressure, MAX_PRESS_LEVEL); // Atualiza a matriz de LEDs com o nível de pressão
            }
            xSemaphoreGive(xReadingsMutex); // Libera o mutex da matriz de LEDs
        }
        vTaskDelay(pdMS_TO_TICKS(225)); // Aguarda 225 ms antes de repetir
    }
}

void vTaskBuzzer()
{
    system_state_t current_state_copy;

    while (1)
    {
        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE)
        {
            current_state_copy = g_current_system_state;
            xSemaphoreGive(xStateMutex);
        }
        if (current_state_copy == SYSTEM_ALERT) // Verifica se o sistema está enchendo
            double_beep();                      // Emite um sinal sonoro de dois

        vTaskDelay(pdMS_TO_TICKS(110));
    }
}

void vTaskControlSystem()
{
    system_state_t current_state_copy = SYSTEM_NORMAL; // Cópia do estado atual do sistema
    int32_t new_temperature;
    int32_t new_pressure;
    int32_t new_humidity;

    while (1)
    {
        // Leitura do BMP280
        bmp280_read_raw(I2C_PORT_0, &raw_temp_bmp, &raw_pressure);
        new_pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params) / 100.0; // Converte a pressão lida do BMP280

        if (xSemaphoreTake(xOffsetMutex, portMAX_DELAY) == pdTRUE)
        {
            new_pressure += g_offset_pressure;
            xSemaphoreGive(xOffsetMutex);
        }

        if (aht20_read(I2C_PORT_1, &data))
        {
            if (xSemaphoreTake(xOffsetMutex, portMAX_DELAY) == pdTRUE)
            {
                new_humidity = data.humidity + g_offset_humidity;
                new_temperature = data.temperature + g_offset_temperature;
                xSemaphoreGive(xOffsetMutex);
            }
        }

        if (xSemaphoreTake(xReadingsMutex, portMAX_DELAY) == pdTRUE)
        {
            g_temperature = new_temperature < MIN_TEMP_LEVEL ? MIN_TEMP_LEVEL : new_temperature > MAX_TEMP_LEVEL ? MAX_TEMP_LEVEL: new_temperature;

            g_pressure = new_pressure < MIN_PRESS_LEVEL ? MIN_PRESS_LEVEL : new_pressure > MAX_PRESS_LEVEL ? MAX_PRESS_LEVEL: new_pressure;

            g_humidity = new_humidity < MIN_HUM_LEVEL ? MIN_HUM_LEVEL : new_humidity > MAX_HUM_LEVEL ? MAX_HUM_LEVEL : new_humidity;

            add_reading(g_temperature, g_temperature_historic_levels);
            add_reading(g_pressure, g_pressure_historic_levels);
            add_reading(g_humidity, g_humidity_historic_levels);
            xSemaphoreGive(xReadingsMutex);
        }

        /* -------------- ATUALIZAÇÃO DO ESTADO DO SISTEMA ---------------  */
        if (xSemaphoreTake(xLevelsLimitsMutex, portMAX_DELAY) == pdTRUE)
        {
            if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE)
            {
                if (new_temperature < g_temperature_min_limit || new_temperature > g_temperature_max_limit ||
                    new_pressure < g_pressure_min_limit || new_pressure > g_pressure_max_limit ||
                    new_humidity < g_humidity_min_limit || new_humidity > g_humidity_max_limit)
                {
                    g_current_system_state = SYSTEM_ALERT;       // Muda o estado do sistema para alerta
                    current_state_copy = g_current_system_state; // Atualiza a cópia do estado atual
                }
                else
                {
                    g_current_system_state = SYSTEM_NORMAL;      // Muda o estado do sistema para alerta
                    current_state_copy = g_current_system_state; // Atualiza a cópia do estado atual
                }
                xSemaphoreGive(xStateMutex); // Libera o mutex do estado
            }
            xSemaphoreGive(xLevelsLimitsMutex);
        }

        switch (current_state_copy)
        {
        case SYSTEM_NORMAL:       // Estado de drenagem
            set_led_color(GREEN); // Liga o LED verde
            break;
        case SYSTEM_ALERT:      // Estado de enchimento
            set_led_color(RED); // Liga o LED laranja
            break;
        default: // Estado desconhecido
            printf("WARNING: Unknow state detected! Resetting system to draining state...\n");
            set_led_color(DARK); // Desliga o LED
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Aguarda 500 ms antes de repetir
    }
}

void vTaskTCPServer()
{
    run_tcp_server_loop(); // Inicia o loop do servidor TCP
    deinit_cyw43();        // Finaliza a arquitetura CYW43
}

int main()
{
    init_system_config(); // Função para inicializar a configuração do sistema

    configure_leds();       // Configura os LEDs
    configure_buzzer();     // Configura o buzzer
    configure_led_matrix(); // Configura a matriz de LEDs

    configure_button(BUTTON_A); // Configura os botões
    configure_button(BUTTON_B);
    configure_button(BUTTON_J);

    // Inicializa o BMP280
    bmp280_init(I2C_PORT_0);
    bmp280_get_calib_params(I2C_PORT_0, &params);

    // Inicializa o AHT20
    aht20_reset(I2C_PORT_1);
    aht20_init(I2C_PORT_1);

    // Configura interrupções para os botões
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_J, GPIO_IRQ_EDGE_FALL, true);

    xDisplayMode = xSemaphoreCreateBinary();     // Semáforo binário para o botão J
    xResetThresholds = xSemaphoreCreateBinary(); // Semáforo binário para o botão J

    // Mutexs para controle de acesso e sincronização
    xReadingsMutex = xSemaphoreCreateMutex();
    xLevelsLimitsMutex = xSemaphoreCreateMutex();
    xStateMutex = xSemaphoreCreateMutex();
    xOffsetMutex = xSemaphoreCreateMutex();     // Mutex para controle de offsets
    set_led_color(GREEN);                       // Liga o LED verde
    printf("Iniciando...");                     // Exibe a mensagem inicial no display
    init_cyw43();                               // Inicializa a arquitetura CYW43
    printf("Conectando a rede...");             // Exibe a mensagem inicial no display
    connect_to_wifi();                          // Conecta ao Wi-Fi
    printf("Iniciando servidor...");            // Exibe a mensagem inicial no display
    struct tcp_pcb *server = init_tcp_server(); // Inicializa o servidor TCP

    printf("IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr)); // Exibe o IP no console

    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP)
        printf("Conectado ao Wi-Fi!\n"); // Exibe a mensagem de conexão bem-sucedida

    // Criação das tarefas
    xTaskCreate(vTaskLedMatrix, "Task LED Matrix", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskControlSystem, "Task Control System", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskResetThresholds, "Task Reset Thresholds", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    // xTaskCreate(vTaskBuzzer, "Task Buzzer", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskTCPServer, "Task TCP Server", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    vTaskStartScheduler(); // Inicia o escalonador de tarefas

    panic_unsupported(); // Se o escalonador falhar, entra em pânico
}
