// Thời gian (TV1): SNTP, múi giờ Việt Nam UTC+7.
#include <stdlib.h>
#include <time.h>
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include "platform.h"
#include "platform_internal.h"

#define VN_OFFSET_S (7 * 3600)
#define VALID_AFTER 1704067200      // 2024-01-01: trước mốc này coi như chưa có giờ

void time_start(void)
{
    setenv("TZ", "ICT-7", 1);
    tzset();
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&cfg);          // tự đồng bộ khi có mạng
    // TODO(TV1): nếu có RTC DS3231 thì đọc giờ từ RTC lúc khởi động và ghi lại sau mỗi lần đồng bộ
}

bool time_is_synced(void)
{
    return time(NULL) > VALID_AFTER;
}

int32_t time_today(void)
{
    time_t now = time(NULL);
    if (now <= VALID_AFTER) return -1;
    return (int32_t) ((now + VN_OFFSET_S) / 86400);
}
