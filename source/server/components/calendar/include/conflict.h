// conflict.h: hợp đồng của TV4 (README mục A7). Không đổi chữ ký hàm khi chưa được cả nhóm duyệt.
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

// So các buổi ứng viên với các buổi đang có của user_id (sched_expand trong cùng khoảng ngày).
// Bỏ qua buổi đang được sửa (source_id == ignore_source_id và cùng orig_day với ứng viên), buổi đã huỷ hoặc nghỉ lễ.
// Các buổi khác của cùng lịch vẫn được so. Thời gian di chuyển chỉ xét với buổi liền trước và liền sau ứng viên.
// Trả số xung đột ghi vào out (tối đa max).
int conflict_check(uint16_t user_id, const occurrence_t *candidates, int n, uint32_t ignore_source_id,
                   conflict_t *out, int max);

#ifdef ESP_PLATFORM
void calendar_init(void);       // nạp /data/conflict_ack.json và đăng ký lệnh calendar.*, conflict.*
#endif

#ifdef __cplusplus
}
#endif
