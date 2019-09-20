//This file is just the UDP server example that takes the recieved data and outputs it to connected shift registers
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "driver/gpio.h"

#define PORT CONFIG_PORT

#define GPIO_OUTPUT_IO_DATA     CONFIG_DATA_PIN
#define GPIO_OUTPUT_IO_CLK      CONFIG_CLK_PIN
#define GPIO_OUTPUT_IO_LATCH    CONFIG_LATCH_PIN
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_DATA) | (1ULL<<GPIO_OUTPUT_IO_LATCH) | (1ULL<<GPIO_OUTPUT_IO_CLK))

int shiftOut(char c);

static const char *TAG = "Nixie";

static void udp_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1) {

#ifdef CONFIG_IPV4
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else // IPV6
        struct sockaddr_in6 dest_addr;
        bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
        inet6_ntoa_r(dest_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket bound, port %d", PORT);

        //GPIO config

        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        gpio_config(&io_conf);

        while (1) {

            // ESP_LOGI(TAG, "Waiting for data");
            struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                // ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                // ESP_LOGI(TAG, "%s", rx_buffer);

                for (int i=0; i<len; ++i)
                {
                    if (rx_buffer[i] == ':') {
                        rx_buffer[i] = '6';
                        // ESP_LOGI(TAG, "Changed a \":\" to \"5\" for displaying");
                    }
                }

                gpio_set_level(GPIO_OUTPUT_IO_LATCH, 0);
                // ESP_LOGI(TAG, "Latch (GPIO%i) pulled low", GPIO_OUTPUT_IO_LATCH);
                for (int i=len-2; i>=0; --i)
                {
                    shiftOut(rx_buffer[i]);
                }
                gpio_set_level(GPIO_OUTPUT_IO_LATCH, 1);
                // ESP_LOGI(TAG, "Latch (GPIO%i) pulled high", GPIO_OUTPUT_IO_LATCH);

                int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}

int shiftOut(char c)
{
    // ESP_LOGI(TAG, "Shifting out \"%c\" MSB-first", c);
    for (int i=7; i>=0; --i) {
        // ESP_LOGI(TAG, "Pulling data (GPIO%i) to bit %i, which is bit %i, counting from LSB", GPIO_OUTPUT_IO_DATA, (c>>i)&1, i);
        gpio_set_level(GPIO_OUTPUT_IO_DATA, (c>>i)&1);
        // ESP_LOGI(TAG, "Pulling clock (GPIO%i) high", GPIO_OUTPUT_IO_CLK);
        gpio_set_level(GPIO_OUTPUT_IO_CLK, 1);
        // ESP_LOGI(TAG, "Pulling clock (GPIO%i) low", GPIO_OUTPUT_IO_CLK);
        gpio_set_level(GPIO_OUTPUT_IO_CLK, 0);
    }

    return 0;
}