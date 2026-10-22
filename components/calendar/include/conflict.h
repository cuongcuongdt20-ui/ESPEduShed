// conflict.h: hợp đồng của TV4 (PHẦN A5 của prompt). Không đổi chữ ký hàm khi chưa được cả nhóm duyệt.
#pragma once
#include <stdint.h>
#include "schedule.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { CONFLICT_OVERLAP = 1, CONFLICT_TRAVEL = 2 } conflict_reason_t;

typedef struct {
    occurrence_t a, b;
    uint16_t     overlap_start, overlap_end;   // CONFLICT_TRAVEL: khoảng thiếu thời gian di chuyển
    uint8_t      reason;                       // conflict_reason_t
} conflict_t;

// So các buổi ứng viên với các buổi đang có (sched_expand trong cùng khoảng ngày).
// Bỏ qua buổi có source_id == ignore_source_id (đang sửa chính lịch đó), buổi đã huỷ hoặc nghỉ lễ.
// Trả số xung đột ghi vào out (tối đa max).
int conflict_check(const occurrence_t *candidates, int n, uint32_t ignore_source_id,
                   conflict_t *out, int max);

#ifdef ESP_PLATFORM
#include "esp_http_server.h"
void calendar_init(void);                        // nạp /data/conflict_ack.json
void calendar_register_routes(httpd_handle_t http);
#endif

#ifdef __cplusplus
}
#endif
