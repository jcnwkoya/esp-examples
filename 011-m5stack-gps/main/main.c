/*

Copyright (c) 2018 Mika Tuupola

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/*
#include <stdio.h>
#include <stdint.h>
#include <string.h>

*/

#include <esp_log.h>
#include <driver/uart.h>

#include "minmea.h"
#include "sdkconfig.h"

#define UART_RX_BUF_SIZE    2048
#define UART_TX_BUF_SIZE    0
#define UART_QUEUE_SIZE     0
#define UART_QUEUE_HANDLE   NULL
#define UART_INTR_FLAGS     0

static const char* TAG = "main";

void uart_init()
{
    uart_config_t uart_config = {
        //.baud_rate = 115200,
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        // .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        // .rx_flow_ctrl_thresh = 122,
    };

    ESP_ERROR_CHECK(
        uart_param_config(UART_NUM_2, &uart_config)
    );
    ESP_ERROR_CHECK(
        uart_driver_install(
            UART_NUM_2,
            UART_RX_BUF_SIZE,
            UART_TX_BUF_SIZE,
            UART_QUEUE_SIZE,
            UART_QUEUE_HANDLE,
            UART_INTR_FLAGS
        )
    );

    ESP_ERROR_CHECK(
        uart_set_pin(
            UART_NUM_2,
            GPIO_NUM_17, // M5Stack TX GPS module pins
            GPIO_NUM_16, // M5Stack RX GPS module pins
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE
        )
    );
}

char *uart_read_line(uart_port_t uart_port) {
    static char line[255];
    char *ptr = line;

    while (true) {

        int16_t bytes_read = uart_read_bytes(
            uart_port,
            (unsigned char *)ptr,
            1,
            portMAX_DELAY
        );

        if (1 == bytes_read) {
            if ('\n' == *ptr) {
                ptr++;
                *ptr = '\0';
                return line;
            }
            ptr++;
        }
    }
}

void uart_read_task(void *params)
{
    ESP_LOGI(TAG, "Reading uart...");
    while(1) {
        char *line = uart_read_line(UART_NUM_2);
        ESP_LOGI(TAG, "%s", line);
        //vTaskDelay(1000 / portTICK_RATE_MS);
    }

    vTaskDelete(NULL);
}

void uart_read_and_parse_task(void *params)
{
    ESP_LOGI(TAG, "Reading and parsing NMEA from uart...");
    while(1) {
        char *line = uart_read_line(UART_NUM_2);

        switch (minmea_sentence_id(line, false)) {
            /* RMC (Recommended Minimum: position, velocity, time) */
            case MINMEA_SENTENCE_RMC: {
                struct minmea_sentence_rmc frame;
                if (minmea_parse_rmc(&frame, line)) {
                    ESP_LOGI(
                        TAG,
                        "$xxRMC: coordinates and speed: (%f,%f) %f",
                        minmea_tocoord(&frame.latitude),
                        minmea_tocoord(&frame.longitude),
                        minmea_tofloat(&frame.speed)
                    );
                } else {
                    ESP_LOGI(TAG, "$xxRMC sentence is not parsed");
                }
            } break;

            /* ZDA (Time & Date - UTC, day, month, year and local time zone) */
            case MINMEA_SENTENCE_ZDA: {
                ESP_LOGI(TAG, "$xxZDA");
            } break;

            /* GGA (Fix Data) */
            case MINMEA_SENTENCE_GGA: {
                ESP_LOGI(TAG, "$xxCGA");
            } break;

            /* VTG (Track made good and Ground speed) */
            case MINMEA_SENTENCE_VTG: {
                ESP_LOGI(TAG, "$xxVTG");
            } break;

            /* GSV (Satellites in view) */
            case MINMEA_SENTENCE_GSV: {
                ESP_LOGI(TAG, "$xxGSV");
            } break;

            /* GST (Pseudorange Noise Statistics) */
            case MINMEA_SENTENCE_GST: {
                ESP_LOGI(TAG, "$xxGST");
            } break;

            /* GLL (Geographic Position: Latitude/Longitude) */
            case MINMEA_SENTENCE_GLL: {
                ESP_LOGI(TAG, "$xxGLL");
            } break;

            /* GSA (DOP and active satellites) */
            case MINMEA_SENTENCE_GSA: {
                ESP_LOGI(TAG, "$xxGSA");
            } break;

            case MINMEA_UNKNOWN: {
                ESP_LOGI(TAG, "$xxxxx sentence is not valid\n");
            } break;

            case MINMEA_INVALID: {
                ESP_LOGI(TAG, "$xxxxx sentence is not valid\n");
            } break;
        }
    }
    vTaskDelete(NULL);
}

void app_main()
{
    uart_init();
    xTaskCreatePinnedToCore(uart_read_and_parse_task, "UART read and parse", 2048, NULL, 1, NULL, 1);
}