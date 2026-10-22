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

// Lưu trữ (ack_store.c, chỉ trên ESP32)
void calendar_acks_load(void);
void calendar_acks_save(void);
