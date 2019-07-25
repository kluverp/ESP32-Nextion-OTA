#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define NEX_UART_TXD              CONFIG_UART_TXD
#define NEX_UART_RXD              CONFIG_UART_RXD
#define NEX_UART_RTS              UART_PIN_NO_CHANGE
#define NEX_UART_CTS              UART_PIN_NO_CHANGE
#define NEX_UART_BUF_SIZE         CONFIG_UART_BUF_SIZE
#define NEX_UART_NUM              CONFIG_UART_NUM
#define NEX_OTA_URL               CONFIG_NEXTION_OTA_URL  /* URL to Nextion firmware */
#define NEX_ACK_TIMEOUT           500000                  /* Nextion ack timeout (500ms) */
#define MAX_HTTP_RECV_BUFFER      4096                    /* Http buffer, same as Nextion packet size (4096) */

#define EXAMPLE_ESP_WIFI_SSID     CONFIG_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS     CONFIG_WIFI_PASS
#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_WIFI_MAXIMUM_RETRY

static const char* TAG = "Nextion OTA";

static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static int s_retry_num = 0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                s_retry_num++;
                ESP_LOGI(TAG,"retry to connect to the AP");
            }
            ESP_LOGI(TAG,"connect to the AP fail\n");
            break;
        }
    default:
        break;
    }
    return ESP_OK;
}

/**
 * Init WiFi in station mode.
 */
void wifi_init_sta()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}


/**
 * Initialize the UART for Nextion communication
 */
static void uart_init(void)
{
	// Configure parameters of an UART driver,
	// communication pins and install the driver
	uart_config_t uart_config = {
		.baud_rate	= 115200,  //57600,
		.data_bits	= UART_DATA_8_BITS,
		.parity		= UART_PARITY_DISABLE,
		.stop_bits	= UART_STOP_BITS_1,
		.flow_ctrl	= UART_HW_FLOWCTRL_DISABLE,
		.rx_flow_ctrl_thresh = 122,
		.use_ref_tick = false,
	};
	uart_param_config(NEX_UART_NUM, &uart_config);
	uart_set_pin(NEX_UART_NUM, NEX_UART_TXD, NEX_UART_RXD, NEX_UART_RTS, NEX_UART_CTS);
	uart_driver_install(NEX_UART_NUM, NEX_UART_BUF_SIZE * 2, 0, 0, NULL, 0);
}

/**
 * Returns data from UART to given buffer
 */
static int nex_ota_uart_read(uint8_t *uart_data, size_t size)
{
	if (!uart_data) return 0;

	memset(uart_data, 0, size);

	// Read data from the UART buffer if present
	return uart_read_bytes(NEX_UART_NUM, uart_data, size, 20 / portTICK_RATE_MS);
}

/**
 * Send command via UART to Nextion.
 */
static int nex_ota_uart_command(char *command)
{
	int len = 100;
	char str[len];
	sprintf(str, "%s\xFF\xFF\xFF", command);

	return uart_write_bytes(NEX_UART_NUM, str, strlen(str));
}

/**
 * Read from UART and wait for Nextion acknowledge.
 * This is "0x05" when sending packets.
 */
static int nex_ota_uart_wait_ack(int ack)
{
	int ret = 0;

	uint8_t *uart_data = (uint8_t *) malloc(NEX_UART_BUF_SIZE);
	if(!uart_data)
	{
		ESP_LOGE(TAG, "Cannot malloc http uart buffer");
		return ret;
	}

	int64_t time = esp_timer_get_time();

	while (esp_timer_get_time() < time + NEX_ACK_TIMEOUT)
	{
		if (nex_ota_uart_read(uart_data, NEX_UART_BUF_SIZE) && uart_data[0] == ack) {
			ret = 1;
			break;
		}
	}

	free(uart_data);

	return ret;
}

/**
 * Send the upload command, and wait for Nextion ack response.
 */
static int nex_ota_upload_command(int filesize, int baud)
{
	char cmd[100];
	sprintf(cmd, "whmi-wri %d,%d,0", filesize, baud);

	nex_ota_uart_command("");
	nex_ota_uart_command(cmd);

	vTaskDelay(100 / portTICK_PERIOD_MS);

	return nex_ota_uart_wait_ack(0x05);
}

void nex_ota_config_default(esp_http_client_config_t *config)
{
	config->url = 0;
	config->host = 0;
	config->port = 0;
	config->username = 0;
	config->password = 0;
	config->auth_type = HTTP_AUTH_TYPE_NONE;
	config->path = 0;
	config->query = 0;
	config->cert_pem = 0;
	config->method = HTTP_METHOD_GET;
	config->timeout_ms = 0;
	config->disable_auto_redirect = 0;
	config->max_redirection_count = 0;
	config->event_handler = 0;
	config->transport_type = HTTP_TRANSPORT_UNKNOWN;
	config->buffer_size = 0;
	config->user_data = 0;
	config->is_async = 0;
}

static esp_http_client_handle_t nex_ota_http_request()
{
	esp_err_t err;
	esp_http_client_config_t config;
	nex_ota_config_default(&config);
	config.url = NEX_OTA_URL;

	// create client
	esp_http_client_handle_t client = esp_http_client_init(&config);

	esp_http_client_set_header(client, "Content-Type", "application/json");
	//esp_http_client_set_header(client, "Authorization", " Bearer $2y$10$9ij0QHtsrwRDVS53IznV1e95Av.psw53AzRcZ96AMCj.yEeRW/6SO");

	if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
	    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
	    return 0;
	}

	return client;
}

static void nex_ota_clean_http_client(esp_http_client_handle_t client)
{
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
}

static int nex_ota_send_bytes(esp_http_client_handle_t client, int content_length)
{
	int ret            = 1;
	int total_read_len = 0;
	int read_len       = 0;

	char *buffer = (char*)malloc(MAX_HTTP_RECV_BUFFER + 1);
	if (buffer == NULL) {
	    ESP_LOGE(TAG, "Cannot malloc http receive buffer");
	    return 0;
	}

	while (total_read_len < content_length)
	{
	    read_len = esp_http_client_read(client, buffer, MAX_HTTP_RECV_BUFFER);
	    if (read_len <= 0)
	    {
	        ESP_LOGE(TAG, "Error read data");
	        ret = 0;
	        break;
	    }

	    uart_write_bytes(NEX_UART_NUM, (const char*)buffer, read_len);

	    if(!nex_ota_uart_wait_ack(0x05))
	    {
	    	ESP_LOGE(TAG, "Ack timeout!");
	    	ret = 0;
	    	break;
	    }

	    total_read_len += read_len;
	}

	free(buffer);

	return ret;
}

/**
 * Perform HTTP request and download the firmware into the Nextion.
 */
int nex_ota_upload()
{
	esp_http_client_handle_t client = nex_ota_http_request();
	if (!client) {
		nex_ota_clean_http_client(client);
		return 0;
	}

	int content_length = esp_http_client_fetch_headers(client);

	if (!nex_ota_upload_command(content_length, 115200))
	{
		ESP_LOGE(TAG, "Error sending command!");
		nex_ota_clean_http_client(client);
		return 0;
	}

	if (!nex_ota_send_bytes(client, content_length))
	{
		ESP_LOGE(TAG, "Error sending bytes!");
		nex_ota_clean_http_client(client);
		return 0;
	}

	nex_ota_clean_http_client(client);
	return 1;
}

void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    // init UART
	uart_init();

	while(1)
	{
		// wait for WiFi connection
		xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY );

		vTaskDelay(200 / portTICK_PERIOD_MS);

		// connect to webserver and upload Nextion firmware
		if (!nex_ota_upload())
		{
			ESP_LOGE(TAG, "Error uploading firmware!");
		}
		else
		{
			ESP_LOGI(TAG, "Done!");
		}


		vTaskDelay(10000 / portTICK_PERIOD_MS);
	}
}


