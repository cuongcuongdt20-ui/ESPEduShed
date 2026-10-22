// Chống dò mật khẩu (TV1): một IP sai LIMITER_MAX_FAILURES lần trong LIMITER_WINDOW_MS thì bị chặn
// LIMITER_BLOCK_MS. Đăng nhập đúng thì xoá số lần sai. Logic thuần, test được trên máy tính.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define LIMITER_MAX_FAILURES 5
#define LIMITER_WINDOW_MS    60000
#define LIMITER_BLOCK_MS     60000
#define LIMITER_SLOTS        16

void limiter_init(void);
bool limiter_is_blocked(uint32_t ip, int64_t now_ms);
void limiter_failure(uint32_t ip, int64_t now_ms);
void limiter_success(uint32_t ip);
