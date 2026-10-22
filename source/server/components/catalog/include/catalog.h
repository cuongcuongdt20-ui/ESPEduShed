// catalog.h: hợp đồng của TV2 (README mục A7). Không đổi chữ ký hàm khi chưa được cả nhóm duyệt.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool     catalog_school_exists(uint16_t school_id);
bool     catalog_period_to_time(uint16_t school_id, uint8_t first_period, uint8_t count,
                                uint16_t *start_min, uint16_t *end_min);   // false nếu tiết không có
bool     catalog_is_holiday(int32_t day, uint16_t school_id, char name_out[65]);
uint16_t catalog_travel_minutes(uint16_t campus_a, uint16_t campus_b);     // 0 nếu cùng cơ sở
bool     catalog_school_label(uint16_t school_id, char short_name[17], char color[8]);

#ifdef ESP_PLATFORM
void catalog_init(void);        // nạp /data/catalog.json và đăng ký lệnh school.*, periods.*, ...
#endif

#ifdef __cplusplus
}
#endif
