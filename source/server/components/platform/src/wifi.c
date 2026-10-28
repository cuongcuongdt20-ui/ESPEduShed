// Wi-Fi (TV1).
//  - Có cấu hình (NVS, hoặc mặc định trong menuconfig): chế độ STA, mất sóng thì tự nối lại sau 2 giây.
//  - Chưa có cấu hình, hoặc giữ nút BOOT 3 giây lúc khởi động: phát AP "ESPEduShed-XXXX" (192.168.4.1).
//    Client nối vào AP rồi gửi lệnh system.wifi {ssid, pass} qua TCP cổng 5000; ESP lưu NVS và khởi động lại.
//    Vào AP bằng nút BOOT khi đã có cấu hình: sau 10 phút tự khởi động lại về STA.
#include <string.h>
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "platform.h"
#include "platform_internal.h"

static const char *TAG = "wifi";

#define AP_TIMEOUT_US      (10LL * 60 * 1000 * 1000)
#define RECONNECT_DELAY_US (2 * 1000 * 1000)

static bool               s_ap;
static esp_netif_t       *s_sta_netif, *s_ap_netif;
static esp_timer_handle_t s_reconnect_timer;

static void reconnect_cb(void *arg)
{
    esp_wifi_connect();
}

static void restart_cb(void *arg)
{
    esp_restart();
}

static void restart_after(int64_t delay_us)
{
    esp_timer_handle_t t;
    esp_timer_create_args_t args = {.callback = restart_cb, .name = "restart"};
    if (esp_timer_create(&args, &t) == ESP_OK) esp_timer_start_once(t, delay_us);
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = data;
        ESP_LOGW(TAG, "mat ket noi Wi-Fi (ly do %d), noi lai sau 2 giay", d->reason);
        esp_timer_stop(s_reconnect_timer);
        esp_timer_start_once(s_reconnect_timer, RECONNECT_DELAY_US);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "co IP " IPSTR ", TCP cong %d", IP2STR(&e->ip_info.ip), CONFIG_LD_TCP_PORT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "mot thiet bi da noi vao AP cai dat");
    }
}

static bool load_credentials(char ssid[33], char pass[65])
{
    nvs_handle_t h;
    size_t ls = 33, lp = 65;
    bool ok = false;
    if (nvs_open("wifi", NVS_READONLY, &h) == ESP_OK) {
        ok = nvs_get_str(h, "ssid", ssid, &ls) == ESP_OK && ssid[0];
        if (ok && nvs_get_str(h, "pass", pass, &lp) != ESP_OK) pass[0] = 0;
        nvs_close(h);
    }
    if (!ok && strlen(CONFIG_LD_WIFI_SSID) > 0) {
        strlcpy(ssid, CONFIG_LD_WIFI_SSID, 33);
        strlcpy(pass, CONFIG_LD_WIFI_PASS, 65);
        ok = true;
    }
    return ok;
}

// Giữ nút BOOT (mức thấp) liên tục 3 giây lúc khởi động
static bool boot_button_held(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << CONFIG_LD_BOOT_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    if (gpio_get_level(CONFIG_LD_BOOT_BUTTON_GPIO) != 0) return false;
    ESP_LOGI(TAG, "dang giu nut BOOT, giu them 3 giay de vao che do AP...");
    for (int i = 0; i < 60; i++) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (gpio_get_level(CONFIG_LD_BOOT_BUTTON_GPIO) != 0) return false;
    }
    return true;
}

static void start_ap(bool keep_sta)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    wifi_config_t ap = {0};
    int n = snprintf((char *) ap.ap.ssid, sizeof(ap.ap.ssid), "ESPEduShed-%02X%02X", mac[4], mac[5]);
    ap.ap.ssid_len = (uint8_t) n;
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    if (strlen(CONFIG_LD_AP_PASS) >= 8) {
        strlcpy((char *) ap.ap.password, CONFIG_LD_AP_PASS, sizeof(ap.ap.password));
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap.ap.authmode = WIFI_AUTH_OPEN;
        ESP_LOGW(TAG, "LD_AP_PASS ngan hon 8 ky tu: AP khong mat khau");
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(keep_sta ? WIFI_MODE_APSTA : WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    s_ap = true;
    ESP_LOGW(TAG, "che do AP: Wi-Fi \"%s\", mat khau \"%s\", server 192.168.4.1:%d", ap.ap.ssid,
             CONFIG_LD_AP_PASS, CONFIG_LD_TCP_PORT);
}

void wifi_init(void)
{
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));   // cấu hình tự quản lý trong NVS namespace "wifi"
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    esp_timer_create_args_t targs = {.callback = reconnect_cb, .name = "wifi_reconnect"};
    ESP_ERROR_CHECK(esp_timer_create(&targs, &s_reconnect_timer));

    char ssid[33] = "", pass[65] = "";
    bool have = load_credentials(ssid, pass);
    bool forced = boot_button_held();
    if (!have || forced) {
        start_ap(false);
        if (have) {
            ESP_LOGW(TAG, "AP tu tat sau 10 phut");
            restart_after(AP_TIMEOUT_US);
        }
    } else {
        wifi_config_t sta = {0};
        strlcpy((char *) sta.sta.ssid, ssid, sizeof(sta.sta.ssid));
        strlcpy((char *) sta.sta.password, pass, sizeof(sta.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
        ESP_LOGI(TAG, "noi Wi-Fi \"%s\"", ssid);
    }
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool wifi_is_ap(void)
{
    return s_ap;
}

void wifi_ip_str(char out[16])
{
    esp_netif_ip_info_t ip = {0};
    esp_netif_get_ip_info(s_ap ? s_ap_netif : s_sta_netif, &ip);
    esp_ip4addr_ntoa(&ip.ip, out, 16);
}

bool wifi_rssi(int8_t *rssi)
{
    wifi_ap_record_t ap;
    if (s_ap || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return false;
    *rssi = ap.rssi;
    return true;
}

// system.wifi {ssid, pass}: không cần đăng nhập ở chế độ AP (máy mới cài đặt), ở chế độ STA phải đăng nhập
static int cmd_wifi(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    if (!s_ap && ctx->user_id == 0)
        return ld_fail(out, LD_ERR_UNAUTHORIZED, "Phai dang nhap de doi Wi-Fi khi khong o che do AP", NULL);
    const char *ssid = ld_get_str(data, "ssid");
    const char *pass = ld_get_str(data, "pass");
    if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 32) return ld_fail(out, LD_ERR_BAD_ARG, "ssid 1..32 ky tu", "ssid");
    if (!pass) pass = "";
    if (strlen(pass) > 64 || (strlen(pass) > 0 && strlen(pass) < 8))
        return ld_fail(out, LD_ERR_BAD_ARG, "Mat khau Wi-Fi rong hoac 8..64 ky tu", "pass");

    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READWRITE, &h) != ESP_OK) return ld_fail(out, LD_ERR_INTERNAL, "Loi NVS", NULL);
    bool ok = nvs_set_str(h, "ssid", ssid) == ESP_OK && nvs_set_str(h, "pass", pass) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    if (!ok) return ld_fail(out, LD_ERR_INTERNAL, "Khong luu duoc cau hinh Wi-Fi", NULL);

    ESP_LOGW(TAG, "da luu Wi-Fi \"%s\", khoi dong lai sau 1,5 giay", ssid);
    cJSON_AddBoolToObject(out, "restarting", true);
    restart_after(1500 * 1000);               // để kịp gửi phản hồi trước khi khởi động lại
    return LD_OK;
}

void wifi_register_cmds(void)
{
    ld_register_cmd("system.wifi", cmd_wifi, LD_ROLE_NONE);
}
