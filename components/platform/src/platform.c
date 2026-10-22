// Khởi tạo nền tảng (TV1).
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "platform.h"
#include "platform_internal.h"

static const char *TAG = "platform";

void platform_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (storage_mount() != ESP_OK) ESP_LOGE(TAG, "khong mount duoc LittleFS");
    auth_init();
    wifi_start();
    time_start();
    mdns_start();
    ESP_ERROR_CHECK(http_start());
}

void platform_register_routes(httpd_handle_t http)
{
    auth_register(http);
    system_register(http);
    ws_register(http);
}

void platform_register_static(httpd_handle_t http)
{
    static_register(http);
}
