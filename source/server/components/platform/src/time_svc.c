// Thời gian (TV1): SNTP, múi giờ Việt Nam UTC+7, đồng hồ đơn điệu.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "platform.h"
#include "platform_internal.h"

#define VN_OFFSET_S (7 * 3600)
#define VALID_AFTER 1704067200      // 2024-01-01: trước mốc này coi như chưa có giờ

int64_t ld_now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

void time_init(void)
{
    setenv("TZ", "ICT-7", 1);
    tzset();
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    if (esp_netif_sntp_init(&cfg) != ESP_OK) ESP_LOGW("time", "khong khoi dong duoc SNTP");
    // SNTP tự đồng bộ khi có mạng (chế độ STA). Ở chế độ AP không có Internet, time_today() trả -1.
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
