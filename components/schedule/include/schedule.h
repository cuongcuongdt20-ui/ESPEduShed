// schedule.h: hợp đồng của TV3 (PHẦN A5 của prompt). Không đổi chữ ký hàm khi chưa được cả nhóm duyệt.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OCC_TEACH = 1, OCC_TASK = 2, OCC_MAKEUP = 3 } occ_kind_t;
typedef enum { OCC_NORMAL = 0, OCC_CHANGED = 1, OCC_CANCELLED = 2, OCC_HOLIDAY = 3 } occ_status_t;

typedef struct {
    uint32_t source_id;          // id lịch dạy hoặc công việc
    uint8_t  kind;               // occ_kind_t
    uint8_t  status;             // occ_status_t
    int32_t  day;                // ngày thực tế diễn ra
    int32_t  orig_day;           // ngày gốc của buổi, khoá ngoại lệ
    uint16_t start_min;
    uint16_t end_min;
    uint16_t school_id;          // 0 nếu không thuộc trường nào
    uint16_t campus_id;          // 0 nếu không có
} occurrence_t;

#define SCHED_MAX_RANGE_DAYS 62

// Sinh các buổi trong [from, to] (tính cả hai đầu), sắp theo ngày rồi giờ.
// Trả số buổi; -1 nếu vượt max hoặc khoảng quá SCHED_MAX_RANGE_DAYS.
int  sched_expand(int32_t from, int32_t to, occurrence_t *out, int max);
bool sched_label(uint32_t source_id, uint8_t kind, char title[65], char room[17], char class_code[17]);
bool sched_uses_school(uint16_t school_id);      // TV2 gọi trước khi xoá trường hoặc cơ sở
bool sched_uses_class(uint16_t class_id);        // TV2 gọi trước khi xoá lớp

#ifdef ESP_PLATFORM
#include "esp_http_server.h"
void schedule_init(void);                        // nạp /data/schedule.json
void schedule_register_routes(httpd_handle_t http);
#endif

#ifdef __cplusplus
}
#endif
