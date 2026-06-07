#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdio.h>

#include "esp_log.h"

#include "esp_http_client.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"     
#include "esp_timer.h"        


static const char *TAG = "MOTOR_CONTROL";

// Пины для моторов
#define ENA_PIN 17
#define ENB_PIN 18

#define IN1_PIN 11
#define IN2_PIN 12
#define IN3_PIN 21
#define IN4_PIN 14

// GPIO для ультразвука
#define TRIG_GPIO 5
#define ECHO_GPIO 20

#define BUZZER_GPIO GPIO_NUM_2

#define RELAY_PIN GPIO_NUM_46

// Топик для управления моторами
#define MQTT_CONTROL_TOPIC "iot-2/evt/distance/"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#ifndef LEDC_HIGH_SPEED_MODE
#define LEDC_HIGH_SPEED_MODE 0
#endif

#ifndef LEDC_LOW_SPEED_MODE
#define LEDC_LOW_SPEED_MODE 1
#endif

// Каналы PWM
#define PWM_FREQ 1000
#define PWM_RESOLUTION LEDC_TIMER_8_BIT  // 8 бит разрешения (0-255)
#define CHANNEL_A 0
#define CHANNEL_B 1

// Максимальная и минимальная скорость PWM
#define MAX_DUTY 255
#define MIN_DUTY 0

static EventGroupHandle_t s_wifi_event_group;
esp_mqtt_client_handle_t mqtt_client = NULL;

// Флаги и переменные

volatile bool is_moving = false; // Управление движением
TaskHandle_t distance_task_handle = NULL;

// Флаг для запуска моторов
volatile int motor_running = 0;

void setup_pwm() {
    // Настройка таймера
     ESP_LOGI("PWM_SETUP", "Начало настройки PWM");
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    //ledc_timer_config(&ledc_timer);
    esp_err_t err = ledc_timer_config(&ledc_timer);
     if (err == ESP_OK) {
        ESP_LOGI("PWM_SETUP", "Таймер настроен успешно");
    } else {
        ESP_LOGE("PWM_SETUP", "Ошибка настройки таймера: %d", err);
    }

    // Назначение каналов и пинов для мотора А
    ledc_channel_config_t ledc_channel_a = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = CHANNEL_A,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = ENA_PIN,
        .duty = 0,
        .hpoint = 0
    };
    //ledc_channel_config(&ledc_channel_a);
    err = ledc_channel_config(&ledc_channel_a);
    if (err == ESP_OK) {
        ESP_LOGI("PWM_SETUP", "Канал A настроен успешно на пине %d", ENA_PIN);
    } else {
        ESP_LOGE("PWM_SETUP", "Ошибка настройки канала A: %d", err);
    }

    // Назначение каналов и пинов для мотора B
    ledc_channel_config_t ledc_channel_b = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = CHANNEL_B,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = ENB_PIN,
        .duty = 0,
        .hpoint = 0
    };
    //ledc_channel_config(&ledc_channel_b);
    err = ledc_channel_config(&ledc_channel_b);
    if (err == ESP_OK) {
        ESP_LOGI("PWM_SETUP", "Канал B настроен успешно на пине %d", ENB_PIN);
    } else {
        ESP_LOGE("PWM_SETUP", "Ошибка настройки канала B: %d", err);
    }

    ESP_LOGI("PWM_SETUP", "Настройка PWM завершена");
}


// Функция для установки скорости PWM для мотора
void set_speed(int channel, uint8_t duty) {
    ESP_LOGI("PWM_SET_SPEED", "Установка скорости: канал=%d, duty=%d", channel, duty);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
    ESP_LOGI("PWM_SET_SPEED", "Скорость установлена: канал=%d, duty=%d", channel, duty);
}

// Управление моторами
void start_motors(uint8_t speed) {
    // Включение моторов
    gpio_set_level((gpio_num_t)IN1_PIN, 1);
    gpio_set_level((gpio_num_t)IN2_PIN, 0);
    set_speed(CHANNEL_A, speed);

    gpio_set_level((gpio_num_t)IN3_PIN, 1);
    gpio_set_level((gpio_num_t)IN4_PIN, 0);
    set_speed(CHANNEL_B, speed);

    // Включить реле при движении
    gpio_set_level(RELAY_PIN, 1);
}


void stop_motors() {
    gpio_set_level((gpio_num_t)IN1_PIN, 0);
    gpio_set_level((gpio_num_t)IN2_PIN, 0);
    set_speed(CHANNEL_A, 0);
    gpio_set_level((gpio_num_t)IN3_PIN, 0);
    gpio_set_level((gpio_num_t)IN4_PIN, 0);
    set_speed(CHANNEL_B, 0);

    gpio_set_level(RELAY_PIN, 0);
}

void start_motors_backward(uint8_t speed) {
    // Включение моторов для движения назад
    gpio_set_level((gpio_num_t)IN1_PIN, 0);
    gpio_set_level((gpio_num_t)IN2_PIN, 1);
    set_speed(CHANNEL_A, speed);

    gpio_set_level((gpio_num_t)IN3_PIN, 0);
    gpio_set_level((gpio_num_t)IN4_PIN, 1);
    set_speed(CHANNEL_B, speed);
}



// Функция для движения назад на определенное расстояние (примерная реализация)
void move_backward_until_distance(int distance_cm) {
    int traveled_distance = 0;
    start_motors_backward(255);
    while (traveled_distance < distance_cm) {
       
        vTaskDelay(pdMS_TO_TICKS(50));
        // Обновляем пройденное расстояние
        traveled_distance += 5;
    }
    stop_motors();
}




// Вращение налево
void start_turn_left(uint8_t speed) {
    gpio_set_level((gpio_num_t)IN1_PIN, 1);
    gpio_set_level((gpio_num_t)IN2_PIN, 0);
    set_speed(CHANNEL_A, speed);

    gpio_set_level((gpio_num_t)IN3_PIN, 0);
    gpio_set_level((gpio_num_t)IN4_PIN, 1);
    set_speed(CHANNEL_B, speed);
}

// Вращение направо
void start_turn_right(uint8_t speed) {
    gpio_set_level((gpio_num_t)IN1_PIN, 0);
    gpio_set_level((gpio_num_t)IN2_PIN, 1);
    set_speed(CHANNEL_A, speed);

    gpio_set_level((gpio_num_t)IN3_PIN, 1);
    gpio_set_level((gpio_num_t)IN4_PIN, 0);
    set_speed(CHANNEL_B, speed);
}

// Поворот налево на заданный угол (или время)
void turn_left_until_angle_or_time(int angle_deg) {
    start_turn_left(255);
   
    vTaskDelay(pdMS_TO_TICKS(7000)); 
    stop_motors();
}

// Поворот направо на заданный угол 
void turn_right_until_angle_or_time(int angle_deg) {
    start_turn_right(255);
   
    vTaskDelay(pdMS_TO_TICKS(7000)); 
    stop_motors();
}



void distance_measurement_task(void *pvParameters) {
    const float obstacle_distance_threshold = 70.0f; // порог для препятствия
    const int back_off_distance_cm = 100; // расстояние, на которое отъезжаем назад
    ///const int turn_angle_threshold = 90; // угол поворота налево в градусах
    const int turn_angle_threshold = 100; // угол поворота налево в градусах
    bool turning_left = false;

    while (1) {
        if (is_moving) {
            // Генерация импульса для TRIG
            gpio_set_level(TRIG_GPIO, 0);
            esp_rom_delay_us(2);
            gpio_set_level(TRIG_GPIO, 1);
            esp_rom_delay_us(10);
            gpio_set_level(TRIG_GPIO, 0);

            // Измерение длительности импульса ECHO
            while (gpio_get_level(ECHO_GPIO) == 0);
            int64_t pulse_start = esp_timer_get_time();

            while (gpio_get_level(ECHO_GPIO) == 1);
            int64_t pulse_end = esp_timer_get_time();

            int64_t duration_us = pulse_end - pulse_start;
            float distance_cm = (duration_us / 2.0f) / 29.1f;

            ESP_LOGI(TAG, "Distance: %.2f cm", distance_cm);

            static bool turn_left_next = true;

            // Внутри функции distance_measurement_task в месте, где обнаружено препятствие:
            if (distance_cm < obstacle_distance_threshold) {
                // Остановка
                stop_motors();
                vTaskDelay(pdMS_TO_TICKS(300));

                // Отъезд назад
                start_motors_backward(255);
                vTaskDelay(pdMS_TO_TICKS(500)); // время назад, подберите по необходимости
                stop_motors();
                vTaskDelay(pdMS_TO_TICKS(500));

                // Поворот на 180 градусов
                if (turn_left_next) {
                    // Поворот налево на 180 градусов
                    start_turn_left(255);
                    // Предположим, что 180 градусов — это примерно 100 единиц вращения в вашем коде
                    turn_left_until_angle_or_time(180);
                    stop_motors();
                    turn_left_next = false; // следующий раз поворот будет вправо
                } else {
                   
                    start_turn_right(255);
                    turn_right_until_angle_or_time(180);
                    stop_motors();
                    turn_left_next = true; // следующий раз — влево
                }

                // Продолжить движение вперед
                start_motors(255);
            }
        } else {
            // Не движемся, делаем паузу
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        // Основной цикл паузы
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}




// Инициализация GPIO для пищалки
void init_buzzer() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void init_ultrasound_gpio() {
    gpio_reset_pin(TRIG_GPIO);
    gpio_set_direction(TRIG_GPIO, GPIO_MODE_OUTPUT);
    gpio_reset_pin(ECHO_GPIO);
    gpio_set_direction(ECHO_GPIO, GPIO_MODE_INPUT);
}

// Функция для издания звука
void beep() {
    gpio_set_level(BUZZER_GPIO, 1);  // Включить пищалку
    vTaskDelay(pdMS_TO_TICKS(200));  // Задержка 200 мс
    gpio_set_level(BUZZER_GPIO, 0);  // Выключить пищалку
}

// Обработчик MQTT сообщений
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            esp_mqtt_client_subscribe(mqtt_client, MQTT_CONTROL_TOPIC, 0);
            beep();
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Received topic: %s, data: %.*s", event->topic, event->data_len, event->data);
            if (strncmp(event->topic, MQTT_CONTROL_TOPIC, strlen(MQTT_CONTROL_TOPIC)) == 0) {
                if (event->data_len == 1) {
                    if (event->data[0] == '1') {
                        ESP_LOGI(TAG, "Start motors");
                        motor_running = 1;                      
                        is_moving = true;
                        start_motors(255);
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        if (distance_task_handle == NULL) {
                         xTaskCreate(distance_measurement_task, "distance_task", 4096, NULL, 5, &distance_task_handle);
                        }
                        
                    } else if (event->data[0] == '0') {
                        ESP_LOGI(TAG, "Stop moving");
                        is_moving = false;
                        stop_motors();
                        if (distance_task_handle != NULL) {
                            vTaskDelete(distance_task_handle);
                            distance_task_handle = NULL;
                        }
                    }
                }
            }
            break;
        default:
            break;
    }
}


// Обработчик событий Wi-Fi
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    static int s_retry_num = 0;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* ip_event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected with IP: " IPSTR, IP2STR(&ip_event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Инициализация GPIO
void init_gpio() {
    gpio_reset_pin(IN1_PIN);
    gpio_set_direction((gpio_num_t)IN1_PIN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(IN2_PIN);
    gpio_set_direction((gpio_num_t)IN2_PIN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(IN3_PIN);
    gpio_set_direction((gpio_num_t)IN3_PIN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(IN4_PIN);
    gpio_set_direction((gpio_num_t)IN4_PIN, GPIO_MODE_OUTPUT);


     // Инициализация пина реле
    gpio_reset_pin(RELAY_PIN);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_PIN, 0); // выключено по умолчанию
}

// Инициализация Wi-Fi
void wifi_init_sta(void) {
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
           // .ssid = "TP-Link_489D",
            //.password = "67828456",
            .ssid = "Дарья's Galaxy A54 5G",
            .password = "4umzqjrt",

            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}



void app_main() {

    init_buzzer();
    
    // Инициализация NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Создаем event loop один раз
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Инициализация сетевого интерфейса
    ESP_ERROR_CHECK(esp_netif_init());

    // Инициализация Wi-Fi
    wifi_init_sta();
    
    // Инициализация GPIO
    //init_gpio_motors();
    init_ultrasound_gpio();
  

  
    s_wifi_event_group = xEventGroupCreate();

    // Настройка MQTT
    esp_mqtt_client_config_t mqtt_cfg = {
      
        .broker.address.uri = "mqtt://10.207.145.117:1883",
        .credentials.client_id = "b46866ffa6c649429716cd75c2e7f1d2",
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, &mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    // Инициализация GPIO
    init_gpio();
    setup_pwm();

    // Основной цикл
    while (1) {
       
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


