// API hệ thống (TV1): trạng thái thiết bị, sao lưu, khôi phục.
#include <time.h>
#include "esp_littlefs.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "platform.h"
#include "platform_internal.h"

// GET /api/system/status
static esp_err_t status(httpd_req_t *req)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "now", (double) time(NULL) * 1000.0);
    cJSON_AddBoolToObject(r, "timeSynced", time_is_synced());
    cJSON_AddNumberToObject(r, "today", time_today());
    cJSON_AddNumberToObject(r, "uptimeS", (double) (esp_timer_get_time() / 1000000));
    cJSON_AddNumberToObject(r, "heapFree", esp_get_free_heap_size());

    esp_netif_ip_info_t ip;
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta && esp_netif_get_ip_info(sta, &ip) == ESP_OK) {
        char s[16];
        esp_ip4addr_ntoa(&ip.ip, s, sizeof(s));
        cJSON_AddStringToObject(r, "ip", s);
    }
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) cJSON_AddNumberToObject(r, "rssi", ap.rssi);

    size_t total = 0, used = 0;
    if (esp_littlefs_info("data", &total, &used) == ESP_OK) {
        cJSON_AddNumberToObject(r, "dataTotal", total);
        cJSON_AddNumberToObject(r, "dataUsed", used);
    }
    // TODO(TV1): số client WebSocket đang nối
    return json_reply(req, 200, r);
}

// GET /api/backup: gộp mọi /data/*.json thành một JSON {"schema":1,"files":{"catalog.json":{...},...}}
static esp_err_t backup(httpd_req_t *req)
{
    // TODO(TV1)
    return json_error(req, 501, "NOT_IMPLEMENTED", "Chua lam", NULL);
}

// POST /api/restore: kiểm tra schema, ghi lại từng file bằng store_write_atomic, rồi esp_restart()
static esp_err_t restore(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    // TODO(TV1)
    return json_error(req, 501, "NOT_IMPLEMENTED", "Chua lam", NULL);
}

void system_register(httpd_handle_t http)
{
    http_route(http, "/api/system/status", HTTP_GET, status);
    http_route(http, "/api/backup", HTTP_GET, backup);
    http_route(http, "/api/restore", HTTP_POST, restore);
}
