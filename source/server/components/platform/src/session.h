// Phiên đăng nhập (TV1): token ngẫu nhiên lưu trong RAM. Logic thuần, test được trên máy tính.
// Thời gian dùng đồng hồ đơn điệu (ms từ lúc khởi động), vì phiên mất khi ESP khởi động lại.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SESSION_MAX       16
#define SESSION_TOKEN_LEN 32                                   // 16 byte ngẫu nhiên dạng hex
#define SESSION_TTL_MS    (7LL * 24 * 3600 * 1000)            // 7 ngày

void session_init(void);

// Tạo phiên mới từ 16 byte ngẫu nhiên do bên gọi cung cấp. Đầy thì thay phiên sắp hết hạn nhất.
// Ghi token (33 byte, kết thúc '\0') vào token_out.
void session_create(uint16_t user_id, int64_t now_ms, const uint8_t rnd[16], char token_out[SESSION_TOKEN_LEN + 1]);

// Token còn hạn thì ghi user_id và trả true
bool session_lookup(const char *token, int64_t now_ms, uint16_t *user_id);

void session_remove(const char *token);
void session_remove_user(uint16_t user_id);    // đổi mật khẩu, xoá tài khoản
int  session_count(int64_t now_ms);
