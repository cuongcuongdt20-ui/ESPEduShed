// Điểm khởi động firmware ESPEduShed (TV1). Chỉ khởi tạo và nối các module, không chứa nghiệp vụ.
// Thứ tự: nền tảng → các module đăng ký lệnh → tìm server qua UDP/mDNS → mở TCP server.
#include "esp_log.h"
#include "platform.h"
#include "discovery.h"
#include "catalog.h"
#include "schedule.h"
#include "conflict.h"

void app_main(void)
{
    platform_init();        // TV1: NVS, LittleFS, Wi-Fi, SNTP, tài khoản, lệnh auth.* và system.*

    catalog_init();         // TV2: nạp /data/catalog.json, đăng ký school.*, periods.*, ...
    schedule_init();        // TV3: nạp /data/schedule.json, đăng ký schedule.*, task.*, ...
    calendar_init();        // TV4: nạp /data/conflict_ack.json, đăng ký calendar.*, conflict.*

    discovery_start();      // TV2: UDP 5001 và mDNS espedushed.local
    platform_start();       // TV1: mở TCP 5000, sau khi mọi lệnh đã đăng ký

    ESP_LOGI("app", "ESPEduShed san sang");
}
