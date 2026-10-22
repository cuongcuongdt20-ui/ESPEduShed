// dt.h: thư viện ngày giờ (TV2), chạy được cả trên ESP32 lẫn trên máy tính khi test.
// Ngày = số ngày tính từ 1970-01-01 (int32_t). Giờ = phút từ 00:00 (uint16_t, 0..1439). Thứ: 1 = thứ Hai … 7 = CN.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t dt_from_ymd(int y, int m, int d);
void    dt_to_ymd(int32_t day, int *y, int *m, int *d);
int     dt_weekday(int32_t day);                 // 1..7
int32_t dt_week_start(int32_t day);              // thứ Hai của tuần chứa day
bool    dt_parse_date(const char *s, int32_t *out);   // "YYYY-MM-DD", kiểm tra ngày hợp lệ, năm nhuận
void    dt_format_date(int32_t day, char out[11]);
bool    dt_parse_hm(const char *s, uint16_t *out);     // "HH:MM"
void    dt_format_hm(uint16_t min, char out[6]);

#ifdef __cplusplus
}
#endif
