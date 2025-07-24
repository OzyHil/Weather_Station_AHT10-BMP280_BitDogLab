#include "Webserver.h" // Biblioteca geral do sistema

void init_cyw43()
{
    // Inicializa a arquitetura CYW43
    if (cyw43_arch_init())
    {
        printf("Falha ao inicializar o Wi-Fi");
        sleep_ms(100);
    }

    // Coloca GPIO do módulo em nível baixo
    cyw43_arch_gpio_put(LED_PIN, 0);
}

void connect_to_wifi()
{
    // Ativa o modo Station (cliente Wi-Fi)
    cyw43_arch_enable_sta_mode();

    // Tenta conectar ao Wi-Fi com timeout de 20 segundos
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi");
        sleep_ms(100);
    }

    struct netif *netif = &cyw43_state.netif[0];
    while (netif_is_link_up(netif) == 0)
    {
        sleep_ms(100);
    }
}

struct tcp_pcb *init_tcp_server()
{
    // Cria o controle de protocolo TCP
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        ////////////// Falha ao criar o servidor TCP
        return NULL;
    }

    // Tenta vincular à porta 80
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        ///////////////// Falha ao vincular à porta
        return NULL;
    }

    // Coloca em modo de escuta e define a função de callback
    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);

    return server;
}

void run_tcp_server_loop()
{
    // Loop principal para manter o Wi-Fi ativo e escutar conexões
    while (true)
    {
        cyw43_arch_poll();
        vTaskDelay(100);
    }
}

void deinit_cyw43()
{
    // Finaliza a arquitetura CYW43
    cyw43_arch_deinit();
    vTaskDelete(NULL); // Deleta a tarefa atual
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Tratamento do request do usuário
void user_request(char **request_ptr, struct tcp_pcb *tpcb)
{
    char *request = *request_ptr;

    int32_t humidity;
    int32_t humidity_min_limit;
    int32_t humidity_max_limit;
    int32_t humidity_offset;

    int32_t temperature;
    int32_t temperature_min_limit;
    int32_t temperature_max_limit;
    int32_t temperature_offset;

    int32_t pressure;
    int32_t pressure_min_limit;
    int32_t pressure_max_limit;
    int32_t pressure_offset;

    int32_t humidity_historic_levels[MAX_READINGS];
    int32_t temperature_historic_levels[MAX_READINGS];
    int32_t pressure_historic_levels[MAX_READINGS];

    if (strstr(request, "GET /data"))
    {
        // Geração do JSON de resposta (manual)
        char json[4096];

        if (xSemaphoreTake(xReadingsMutex, portMAX_DELAY) == pdTRUE)
        {
            humidity = g_humidity;
            temperature = g_temperature;
            pressure = g_pressure;

            memcpy(humidity_historic_levels, g_humidity_historic_levels, sizeof(int32_t) * MAX_READINGS);
            memcpy(temperature_historic_levels, g_temperature_historic_levels, sizeof(int32_t) * MAX_READINGS);
            memcpy(pressure_historic_levels, g_pressure_historic_levels, sizeof(int32_t) * MAX_READINGS);
            xSemaphoreGive(xReadingsMutex);
        }

        if (xSemaphoreTake(xLevelsLimitsMutex, portMAX_DELAY) == pdTRUE)
        {
            pressure_min_limit = g_pressure_min_limit;
            pressure_max_limit = g_pressure_max_limit;

            humidity_min_limit = g_humidity_min_limit;
            humidity_max_limit = g_humidity_max_limit;

            temperature_min_limit = g_temperature_min_limit;
            temperature_max_limit = g_temperature_max_limit;

            xSemaphoreGive(xLevelsLimitsMutex);
        }

        if (xSemaphoreTake(xOffsetMutex, portMAX_DELAY) == pdTRUE)
        {
            pressure_offset = g_offset_pressure;
            humidity_offset = g_offset_humidity;
            temperature_offset = g_offset_temperature;
            xSemaphoreGive(xOffsetMutex);
        }

        char json_h_temp[256];
        char *ptr_h_temp = json_h_temp;

        char json_h_hum[256];
        char *ptr_h_hum = json_h_hum;

        char json_h_press[256];
        char *ptr_h_press = json_h_press;

        *ptr_h_temp++ = '[';
        for (int i = 0; i < MAX_READINGS; i++)
        {
            ptr_h_temp += sprintf(ptr_h_temp, "%d%s", temperature_historic_levels[i], (i < MAX_READINGS - 1) ? "," : "");
        }
        *ptr_h_temp++ = ']';
        *ptr_h_temp = '\0';

        *ptr_h_hum++ = '[';
        for (int i = 0; i < MAX_READINGS; i++)
        {
            ptr_h_hum += sprintf(ptr_h_hum, "%d%s", humidity_historic_levels[i], (i < MAX_READINGS - 1) ? "," : "");
        }
        *ptr_h_hum++ = ']';
        *ptr_h_hum = '\0';

        *ptr_h_press++ = '[';
        for (int i = 0; i < MAX_READINGS; i++)
        {
            ptr_h_press += sprintf(ptr_h_press, "%d%s", pressure_historic_levels[i], (i < MAX_READINGS - 1) ? "," : "");
        }
        *ptr_h_press++ = ']';
        *ptr_h_press = '\0';

        snprintf(json, sizeof(json),
                 "{\"temp_offset\":%d,\"pres_offset\":%d,\"humi_offset\":%d,\"temp_atual\":%d,\"hum_atual\":%d,\"press_atual\":%d,\"min_config_temp\":%d,\"max_config_temp\":%d,\"min_config_hum\":%d,\"max_config_hum\":%d,\"min_config_press\":%d,\"max_config_press\":%d,\"historico_temp\":%s,\"historico_hum\":%s,\"historico_press\":%s}",
                 temperature_offset,
                 pressure_offset,
                 humidity_offset,
                 temperature,
                 humidity,
                 pressure,
                 temperature_min_limit,
                 temperature_max_limit,
                 humidity_min_limit,
                 humidity_max_limit,
                 pressure_min_limit,
                 pressure_max_limit,
                 json_h_temp,
                 json_h_hum,
                 json_h_press);

        // Monta resposta HTTP
        char response[1024];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n\r\n"
                 "%s",
                 strlen(json), json);

        tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
    }
    else if (strstr(request, "POST /config"))
    {
        char *body = strstr(request, "\r\n\r\n");
        if (body)
        {
            body += 4; // pular os headers

            // Variáveis para armazenar os valores recebidos
            char sensor[32] = {0};
            int min = 0, max = 0;
            int offset = 0;

            // Parse manual (simples)
            char *min_ptr = strstr(body, "min_value=");
            char *max_ptr = strstr(body, "max_value=");
            char *offset_ptr = strstr(body, "offset_value=");
            char *sensor_ptr = strstr(body, "sensor=");

            if (sensor_ptr)
                sscanf(sensor_ptr, "sensor=%31[^&]", sensor);

            if (min_ptr)
                sscanf(min_ptr, "min_value=%d", &min);

            if (max_ptr)
                sscanf(max_ptr, "max_value=%d", &max);

            if (offset_ptr)
                sscanf(offset_ptr, "offset_value=%d", &offset);

            if (strcmp(sensor, "temperature") == 0)
            {
                if (min >= MIN_TEMP_LEVEL && max <= MAX_TEMP_LEVEL && min < max)
                {
                    if (xSemaphoreTake(xLevelsLimitsMutex, portMAX_DELAY) == pdTRUE)
                    {
                        g_temperature_min_limit = min;
                        g_temperature_max_limit = max;
                        xSemaphoreGive(xLevelsLimitsMutex);

                        if (xSemaphoreTake(xOffsetMutex, portMAX_DELAY) == pdTRUE)
                        {
                            g_offset_temperature = offset;
                            xSemaphoreGive(xOffsetMutex);
                        }
                    }
                    tcp_write(tpcb, "HTTP/1.1 200 OK\r\n\r\n", 19, TCP_WRITE_FLAG_COPY);
                }
                else
                    tcp_write(tpcb, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, TCP_WRITE_FLAG_COPY);
            }
            else if (strcmp(sensor, "humidity") == 0)
            {
                if (min >= MIN_HUM_LEVEL && max <= MAX_HUM_LEVEL && min < max)
                {
                    if (xSemaphoreTake(xLevelsLimitsMutex, portMAX_DELAY) == pdTRUE)
                    {
                        g_humidity_min_limit = min;
                        g_humidity_max_limit = max;
                        xSemaphoreGive(xLevelsLimitsMutex);
                        if (xSemaphoreTake(xOffsetMutex, portMAX_DELAY) == pdTRUE)
                        {
                            g_offset_humidity = offset;
                            xSemaphoreGive(xOffsetMutex);
                        }
                    }
                    tcp_write(tpcb, "HTTP/1.1 200 OK\r\n\r\n", 19, TCP_WRITE_FLAG_COPY);
                }
                else
                    tcp_write(tpcb, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, TCP_WRITE_FLAG_COPY);
            }
            else if (strcmp(sensor, "pressure") == 0)
            {
                if (min >= MIN_PRESS_LEVEL && max <= MAX_PRESS_LEVEL && min < max)
                {
                    if (xSemaphoreTake(xLevelsLimitsMutex, portMAX_DELAY) == pdTRUE)
                    {
                        g_pressure_min_limit = min;
                        g_pressure_max_limit = max;
                        xSemaphoreGive(xLevelsLimitsMutex);
                        if (xSemaphoreTake(xOffsetMutex, portMAX_DELAY) == pdTRUE)
                        {
                            g_offset_pressure = offset;
                            xSemaphoreGive(xOffsetMutex);
                        }
                    }
                    tcp_write(tpcb, "HTTP/1.1 200 OK\r\n\r\n", 19, TCP_WRITE_FLAG_COPY);
                }
                else
                    tcp_write(tpcb, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, TCP_WRITE_FLAG_COPY);
            }

            tcp_output(tpcb);
        }
    }
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Alocação do request na memória dinâmica
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    // Tratamento de request - Controle dos LEDs
    user_request(&request, tpcb);

    char html[8144];

    snprintf(html, sizeof(html),
             "<!DOCTYPE html>\n"
             "<html lang=\"pt-BR\">\n"
             "<head>\n"
             "<meta charset=\"UTF-8\">\n"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
             "<title>Weather Station Dashboard</title>\n"
             "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js\"></script>\n"
             "<style>\n"
             "body {\n"
             "  font-family: Arial, sans-serif;\n"
             "  background: #f4f4f4;\n"
             "  margin: 0;\n"
             "  padding: 20px;\n"
             "}\n"
             "h1 {\n"
             "  text-align: center;\n"
             "  margin-bottom: 30px;\n"
             "}\n"
             ".hidden {\n"
             "  display: none;\n"
             "}\n"
             ".card-container {\n"
             "  display: flex;\n"
             "  justify-content: center;\n"
             "  gap: 20px;\n"
             "  flex-wrap: wrap;\n"
             "  margin-bottom: 30px;\n"
             "}\n"
             ".card {\n"
             "  background: #fff;\n"
             "  padding: 20px;\n"
             "  border-radius: 10px;\n"
             "  box-shadow: 0 2px 6px rgba(0,0,0,0.15);\n"
             "  min-width: 220px;\n"
             "  flex: 1 1 220px;\n"
             "  max-width: 280px;\n"
             "}\n"
             ".card h2 {\n"
             "  font-weight: 600;\n"
             "  font-size: 1.3rem;\n"
             "  margin-bottom: 15px;\n"
             "}\n"
             ".sensor-name {\n"
             "  font-weight: normal;\n"
             "  font-size: 0.9rem;\n"
             "  color: #888;\n"
             "  margin-left: 6px;\n"
             "}\n"
             ".value {\n"
             "  font-size: 2.4rem;\n"
             "  font-weight: bold;\n"
             "  color: #000;\n"
             "  margin-bottom: 8px;\n"
             "}\n"
             ".minmax {\n"
             "  font-size: 1rem;\n"
             "  color: #444;\n"
             "}\n"
             ".charts-container {\n"
             "  display: flex;\n"
             "  justify-content: center;\n"
             "  gap: 20px;\n"
             "  flex-wrap: wrap;\n"
             "  margin-bottom: 40px;\n"
             "}\n"
             ".chart-card {\n"
             "  background: #fff;\n"
             "  padding: 15px;\n"
             "  border-radius: 10px;\n"
             "  box-shadow: 0 2px 6px rgba(0,0,0,0.15);\n"
             "  min-width: 280px;\n"
             "  flex: 1 1 280px;\n"
             "  max-width: 320px;\n"
             "}\n"
             ".chart-card h3 {\n"
             "  text-align: center;\n"
             "  margin-bottom: 10px;\n"
             "  font-weight: 600;\n"
             "}\n"
             "canvas {\n"
             "  width: 100%% !important;\n"
             "  height: 180px !important;\n"
             "}\n"
             "button {\n"
             "  display: block;\n"
             "  margin: 20px auto 0 auto;\n"
             "  padding: 12px 30px;\n"
             "  font-size: 1rem;\n"
             "  border-radius: 8px;\n"
             "  border: none;\n"
             "  background: #1976d2;\n"
             "  color: #fff;\n"
             "  cursor: pointer;\n"
             "  box-shadow: 0 2px 5px rgba(25,118,210,0.6);\n"
             "  transition: background-color 0.3s ease;\n"
             "}\n"
             "button:hover {\n"
             "  background: #155a9a;\n"
             "}\n"
             "form {\n"
             "  background: #fff;\n"
             "  padding: 25px;\n"
             "  border-radius: 10px;\n"
             "  max-width: 400px;\n"
             "  margin: 0 auto;\n"
             "  box-shadow: 0 2px 8px rgba(0,0,0,0.15);\n"
             "  display: flex;\n"
             "  flex-direction: column;\n"
             "  align-items: center;\n"
             "  gap: 15px;\n"
             "}\n"
             "form h2 {\n"
             "  text-align: center;\n"
             "  margin-bottom: 10px;\n"
             "  font-weight: 700;\n"
             "  width: 100%%;\n"
             "}\n"
             "label {\n"
             "  width: 100%%;\n"
             "  font-weight: 600;\n"
             "  color: #333;\n"
             "}\n"
             "select, input[type=number] {\n"
             "  width: 100%%;\n"
             "  padding: 8px 10px;\n"
             "  border-radius: 6px;\n"
             "  border: 1px solid #ccc;\n"
             "  font-size: 1rem;\n"
             "  box-sizing: border-box;\n"
             "}\n"
             "input[type=number]:focus, select:focus {\n"
             "  border-color: #1976d2;\n"
             "  outline: none;\n"
             "  box-shadow: 0 0 6px rgba(25,118,210,0.5);\n"
             "}\n"
             "form button {\n"
             "  width: auto;\n"
             "  margin-top: 10px;\n"
             "  background: #1976d2;\n"
             "  font-weight: 700;\n"
             "  box-shadow: none;\n"
             "  align-self: center;\n"
             "}\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"
             "<h1>Estação Meteorológica</h1>\n"
             "\n"
             "<div id=\"data-section\">\n"
             "  <div class=\"card-container\">\n"
             "    <div class=\"card\">\n"
             "      <h2>Temperatura <span class=\"sensor-name\">(AHT10)</span></h2>\n"
             "      <div class=\"value\" id=\"valueTemp\">0°C</div>\n"
             "      <div class=\"minmax\" id=\"minmaxTemp\">Min: 0°C | Max: 0°C</div>\n"
             "    </div>\n"
             "    <div class=\"card\">\n"
             "      <h2>Umidade <span class=\"sensor-name\">(AHT10)</span></h2>\n"
             "      <div class=\"value\" id=\"valueHumi\">0%%</div>\n"
             "      <div class=\"minmax\" id=\"minmaxHumi\">Min: 0%% | Max: 0%%</div>\n"
             "    </div>\n"
             "    <div class=\"card\">\n"
             "      <h2>Pressão <span class=\"sensor-name\">(BMP280)</span></h2>\n"
             "      <div class=\"value\" id=\"valuePres\">0hPa</div>\n"
             "      <div class=\"minmax\" id=\"minmaxPres\">Min: 0hPa | Max: 0hPa</div>\n"
             "    </div>\n"
             "  </div>\n"
             "\n"
             "  <div class=\"charts-container\">\n"
             "    <div class=\"chart-card\">\n"
             "      <h3>Últimas variações de Temperatura</h3>\n"
             "      <canvas id=\"tempChart\"></canvas>\n"
             "    </div>\n"
             "    <div class=\"chart-card\">\n"
             "      <h3>Últimas variações de Umidade</h3>\n"
             "      <canvas id=\"humChart\"></canvas>\n"
             "    </div>\n"
             "    <div class=\"chart-card\">\n"
             "      <h3>Últimas variações de Pressão</h3>\n"
             "      <canvas id=\"presChart\"></canvas>\n"
             "    </div>\n"
             "  </div>\n"
             "</div>\n"
             "\n"
             "<div id=\"config-section\" class=\"hidden\">\n"
             "  <form id=\"configForm\" method=\"POST\" action=\"/config\">\n"
             "    <h2>Configurar Limiares e Offset</h2>\n"
             "    <label for=\"sensorSelect\">Selecione a grandeza:</label>\n"
             "    <select id=\"sensorSelect\" name=\"sensor\">\n"
             "      <option value=\"temperature\">Temperatura</option>\n"
             "      <option value=\"humidity\">Umidade</option>\n"
             "      <option value=\"pressure\">Pressão</option>\n"
             "    </select>\n"
             "    <label for=\"minInput\">Valor Mínimo:</label>\n"
             "    <input type=\"number\" id=\"minInput\" name=\"min_value\" step=\"1\" value=\"0\" required>\n"
             "    <label for=\"maxInput\">Valor Máximo:</label>\n"
             "    <input type=\"number\" id=\"maxInput\" name=\"max_value\" step=\"1\" value=\"0\" required>\n"
             "    <label for=\"offsetInput\">Offset de Calibração:</label>\n"
             "    <input type=\"number\" id=\"offsetInput\" name=\"offset_value\" step=\"1\" value=\"0\" required>\n"
             "    <button type=\"submit\">Salvar Configuração</button>\n"
             "  </form>\n"
             "</div>\n"
             "\n"
             "<button id=\"toggleBtn\">Ir para Configuração</button>\n"
             "\n"
             "<script>\n"
             "const toggleBtn = document.getElementById('toggleBtn');\n"
             "const dataSection = document.getElementById('data-section');\n"
             "const configSection = document.getElementById('config-section');\n"
             "\n"
             "toggleBtn.addEventListener('click', () => {\n"
             "  if (dataSection.classList.contains('hidden')) {\n"
             "    dataSection.classList.remove('hidden');\n"
             "    configSection.classList.add('hidden');\n"
             "    toggleBtn.textContent = 'Ir para Configuração';\n"
             "  } else {\n"
             "    dataSection.classList.add('hidden');\n"
             "    configSection.classList.remove('hidden');\n"
             "    toggleBtn.textContent = 'Ir para Dados';\n"
             "  }\n"
             "});\n"
             "\n"
             "function createChart(id, label, color, min, max) {\n"
             "  return new Chart(document.getElementById(id), {\n"
             "    type: 'line',\n"
             "    data: {\n"
             "      labels: [],\n"
             "      datasets: [{\n"
             "        label: label,\n"
             "        data: [],\n"
             "        borderColor: color,\n"
             "        backgroundColor: color.replace('rgb', 'rgba').replace(')', ', 0.2)'),\n"
             "        fill: 'start',\n"
             "        tension: 0.4\n"
             "      }]\n"
             "    },\n"
             "    options: {\n"
             "      responsive: true,\n"
             "      plugins: {\n"
             "        legend: { position: 'top' }\n"
             "      },\n"
             "      scales: {\n"
             "        y: { min: min, max: max}\n"
             "      }\n"
             "    }\n"
             "  });\n"
             "}\n"
             "\n"
             "let graficoTemp;\n"
             "let graficoHumi;\n"
             "let graficoPres;\n"
             "\n"
             "function atualizarDados() {\n"
             "  fetch('/data')\n"
             "    .then(res => res.json())\n"
             "    .then(data => {\n"
             "      document.getElementById('valueTemp').innerText = data.temp_atual + '°C';\n"
             "      document.getElementById('minmaxTemp').innerText = 'Min: ' + data.min_config_temp + '°C | Max: ' + data.max_config_temp + '°C | Offset: ' + data.temp_offset + '°C';\n"
             "      document.getElementById('valueHumi').innerText = data.hum_atual + '%%';\n"
             "      document.getElementById('minmaxHumi').innerText = 'Min: ' + data.min_config_hum + '%% | Max: ' + data.max_config_hum + '%% | Offset: ' + data.humi_offset + '%%';\n"
             "      document.getElementById('valuePres').innerText = data.press_atual + 'hPa';\n"
             "      document.getElementById('minmaxPres').innerText = 'Min: ' + data.min_config_press + ' hPa | Max: ' + data.max_config_press + ' hPa | Offset: ' + data.pres_offset + ' hPa';\n"
             "\n"
             "      if (graficoTemp && Array.isArray(data.historico_temp)) {\n"
             "        graficoTemp.data.labels = data.historico_temp.map((_, i) => i + 1);\n"
             "        graficoTemp.data.datasets[0].data = data.historico_temp;\n"
             "        graficoTemp.update();\n"
             "      }\n"
             "      if (graficoHumi && Array.isArray(data.historico_hum)) {\n"
             "        graficoHumi.data.labels = data.historico_hum.map((_, i) => i + 1);\n"
             "        graficoHumi.data.datasets[0].data = data.historico_hum;\n"
             "        graficoHumi.update();\n"
             "      }\n"
             "      if (graficoPres && Array.isArray(data.historico_press)) {\n"
             "        graficoPres.data.labels = data.historico_press.map((_, i) => i + 1);\n"
             "        graficoPres.data.datasets[0].data = data.historico_press;\n"
             "        graficoPres.update();\n"
             "      }\n"
             "    })\n"
             "    .catch(error => console.error('Erro:', error));\n"
             "}\n"
             "\n"
             "graficoTemp = createChart('tempChart', 'Temperatura (°C)', 'rgb(229, 57, 53)', %d, %d);\n"
             "graficoHumi = createChart('humChart', 'Umidade (%%)', 'rgb(251, 140, 0)', %d, %d);\n"
             "graficoPres = createChart('presChart', 'Pressão (hPa)', 'rgb(25, 118, 210)', %d, %d);\n"
             "\n"
             "atualizarDados();\n"
             "setInterval(atualizarDados, 3000);\n"
             "</script>\n"
             "</body>\n"
             "</html>",
            MIN_TEMP_LEVEL, MAX_TEMP_LEVEL,
            MIN_HUM_LEVEL, MAX_HUM_LEVEL,
            MIN_PRESS_LEVEL, MAX_PRESS_LEVEL);

            printf("MIN_TEMP_LEVEL: %d, MAX_TEMP_LEVEL: %d\n", MIN_TEMP_LEVEL, MAX_TEMP_LEVEL);

    // Escreve dados para envio (mas não os envia imediatamente).
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

    // Envia a mensagem
    tcp_output(tpcb);

    // libera memória alocada dinamicamente
    free(request);

    // libera um buffer de pacote (pbuf) que foi alocado anteriormente
    pbuf_free(p);

    return ERR_OK;
}