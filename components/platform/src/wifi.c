// Wi-Fi (TV1). Bản khởi đầu: chế độ STA với SSID trong menuconfig, tự nối lại.
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "platform_internal.h"

static const char *TAG = "wifi";

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "mat ket noi, noi lai");
        esp_wifi_connect();
        // TODO(TV1): mất quá 60 giây thì bật thêm AP dự phòng (WIFI_MODE_APSTA)
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&e->ip_info.ip));
    }
}

void wifi_start(void)
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));

    // TODO(TV1): đọc SSID/mật khẩu từ NVS; NVS trống hoặc giữ nút BOOT 3 giây thì vào chế độ AP
    //            "LichDay-XXXX" (mật khẩu CONFIG_LD_AP_PASS) + captive_dns_start() + trang /setup
    if (strlen(CONFIG_LD_WIFI_SSID) == 0) {
        ESP_LOGE(TAG, "chua cau hinh Wi-Fi: dat LD_WIFI_SSID trong menuconfig (che do AP chua lam)");
        return;
    }
    wifi_config_t wc = {0};
    strlcpy((char *) wc.sta.ssid, CONFIG_LD_WIFI_SSID, sizeof(wc.sta.ssid));
    strlcpy((char *) wc.sta.password, CONFIG_LD_WIFI_PASS, sizeof(wc.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
}
