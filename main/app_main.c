// Điểm khởi động (TV1). Chỉ khởi tạo và nối các module, không chứa nghiệp vụ.
// Thứ tự đăng ký route quan trọng: esp_http_server khớp theo thứ tự đăng ký,
// nên route file tĩnh "/*" phải đăng ký CUỐI CÙNG.
#include "esp_log.h"
#include "platform.h"
#include "catalog.h"
#include "schedule.h"
#include "conflict.h"

static const char *TAG = "app";

void app_main(void)
{
    platform_init();            // NVS, LittleFS (/www, /data), Wi-Fi, SNTP, mDNS, HTTP server

    catalog_init();             // TV2: nạp /data/catalog.json
    schedule_init();            // TV3: nạp /data/schedule.json
    calendar_init();            // TV4: nạp /data/conflict_ack.json

    httpd_handle_t http = platform_http();
    platform_register_routes(http);   // /api/auth, /api/system, /api/backup, /ws
    catalog_register_routes(http);    // /api/schools, /api/holidays, ...
    schedule_register_routes(http);   // /api/schedules, /api/tasks, /api/occurrences
    calendar_register_routes(http);   // /api/calendar, /api/conflicts
    platform_register_static(http);   // "/*": giao diện web, luôn đăng ký cuối

    ESP_LOGI(TAG, "San sang: http://%s.local", CONFIG_LD_HOSTNAME);
}
