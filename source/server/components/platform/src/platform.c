// Khởi động nền tảng (TV1).
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "platform.h"
#include "platform_internal.h"

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

    ESP_ERROR_CHECK(storage_init());
    wifi_init();
    time_init();
    // Sau khi bật Wi-Fi: bộ sinh số ngẫu nhiên phần cứng mới đạt chất lượng cho nonce, token, mật khẩu admin
    auth_init();
    users_init();

    auth_register_cmds();
    system_register_cmds();
    wifi_register_cmds();
}

void platform_start(void)
{
    tcp_server_start();
}
