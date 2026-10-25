// Nội bộ của TV4. Module khác không include file này.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "conflict.h"
#include "ld_lock.h"

#define CAL_MAX_ACKS 128

// Người dùng đã xác nhận "biết rồi" cho một xung đột: khoá (nguồn a, nguồn b, ngày), a < b
typedef struct {
    uint32_t source_a, source_b;
    int32_t  day;
} cal_ack_t;

extern cal_ack_t g_acks[CAL_MAX_ACKS];
extern int       g_n_acks;
extern ld_lock_t g_cal_lock;

// Tìm mọi xung đột trong một danh sách buổi đã sắp theo ngày, giờ (thuật toán quét). Logic thuần.
int  conflict_find_in(const occurrence_t *occ, int n, conflict_t *out, int max);
bool calendar_is_acked(uint32_t a, uint32_t b, int32_t day);

// So hai buổi (a, b bất kỳ thứ tự). Trả true và điền *cf nếu trùng giờ hoặc thiếu thời gian di chuyển. Logic thuần.
bool conflict_pair(const occurrence_t *a, const occurrence_t *b, conflict_t *cf);

// Xác nhận "đã biết" (chỉ dùng trong calendar_cmd.c, chạy trên ESP32)
bool calendar_ack_add(uint32_t a, uint32_t b, int32_t day);   // false nếu bảng đầy

// Lưu trữ (ack_store.c, chỉ trên ESP32)
void calendar_acks_load(void);
void calendar_acks_mark_dirty(void);   // ghi xuống flash sau 1 giây kể từ thay đổi cuối

// Lệnh TCP (calendar_cmd.c, chỉ trên ESP32)
void calendar_register_cmds(void);
