// platform.h: hợp đồng của TV1 cho mọi module (README, mục A7). Không đổi chữ ký hàm khi chưa được cả nhóm duyệt.
//
// Mỗi module đăng ký lệnh TCP của mình bằng ld_register_cmd() trong hàm xxx_init(). Server TCP (TV1) lo phần
// mạng, ghép khung, kiểm tra token và vai trò, rồi gọi handler với ctx đã xác thực.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------- vai trò: số càng lớn quyền càng cao, min_role so sánh bằng >=
#define LD_ROLE_NONE    0      // chưa đăng nhập; min_role = LD_ROLE_NONE nghĩa là lệnh công khai
#define LD_ROLE_STUDENT 1
#define LD_ROLE_TEACHER 2
#define LD_ROLE_ADMIN   3

// ---------- mã lỗi của giao thức (README, mục A3)
typedef enum {
    LD_OK = 0,
    LD_ERR_BAD_JSON,
    LD_ERR_UNKNOWN_CMD,
    LD_ERR_BAD_ARG,
    LD_ERR_UNAUTHORIZED,
    LD_ERR_FORBIDDEN,
    LD_ERR_NOT_FOUND,
    LD_ERR_CONFLICT,
    LD_ERR_FULL,
    LD_ERR_BUSY,
    LD_ERR_INTERNAL,
} ld_err_t;

const char *ld_err_name(int code);   // LD_ERR_CONFLICT → "CONFLICT"

// ---------- ngữ cảnh của một yêu cầu
typedef struct {
    uint16_t    user_id;   // 0 nếu chưa đăng nhập
    uint8_t     role;      // LD_ROLE_*
    int         client;    // chỉ số kết nối trong server (0..LD_MAX_CLIENTS-1)
    uint32_t    ip;        // IPv4 của client (thứ tự byte mạng)
    const char *token;     // token đã xác thực, NULL nếu không có
} ld_ctx_t;

// Handler một lệnh. data: trường "data" của yêu cầu (có thể NULL). out: object "data" của phản hồi, handler thêm
// trường vào đây. Trả LD_OK, hoặc giá trị trả về của ld_fail() khi lỗi.
typedef int (*ld_cmd_fn)(const ld_ctx_t *ctx, const cJSON *data, cJSON *out);

// Đăng ký lệnh, ví dụ ld_register_cmd("school.list", school_list, LD_ROLE_STUDENT). Gọi trước platform_start().
void ld_register_cmd(const char *name, ld_cmd_fn fn, uint8_t min_role);

// Báo lỗi trong handler: return ld_fail(out, LD_ERR_BAD_ARG, "Thiếu tên trường", "name");
// Các trường khác đã thêm vào out (ví dụ "conflicts") vẫn được gửi kèm trong "data" của phản hồi lỗi.
int ld_fail(cJSON *out, int code, const char *message, const char *field);

// Tin đẩy {"push":"changed","entity":..,"id":..}. An toàn khi gọi từ handler hoặc từ task khác.
void ld_push_changed(const char *entity, uint32_t id);                    // mọi client đã đăng nhập
void ld_push_changed_user(uint16_t user_id, const char *entity, uint32_t id);   // chỉ các phiên của một người

// ---------- phân trang cho lệnh trả danh sách (phản hồi phải dưới 8 KB)
// Lệnh nhận {offset?}; thêm từng mục bằng ld_page_add, khi hết ngân sách thì dừng và ghi "next": chỉ số mục tiếp theo.
// Client gọi lại với offset = next cho tới khi phản hồi không còn "next".
#define LD_PAGE_BUDGET 6000
int  ld_page_offset(const cJSON *data);                        // offset hợp lệ (>= 0), mặc định 0
bool ld_page_add(cJSON *arr, cJSON *item, int *budget);        // false: hết chỗ, item đã bị xoá

// ---------- đọc tham số trong "data" (trả false nếu thiếu hoặc sai kiểu)
bool        ld_get_int(const cJSON *data, const char *key, int *out);
bool        ld_get_bool(const cJSON *data, const char *key, bool *out);
const char *ld_get_str(const cJSON *data, const char *key);   // NULL nếu thiếu hoặc không phải chuỗi

// ---------- lưu trữ (LittleFS gắn ở /data)
esp_err_t store_write_atomic(const char *path, const char *data, size_t len);   // ghi file tạm rồi đổi tên
char     *store_read_all(const char *path, size_t *len);                         // caller free(); NULL nếu chưa có
// Ghi và đọc file lớn theo từng dòng (JSON Lines) để không phải dựng cả file trong RAM.
// Ghi: store_begin → store_put_line (mỗi dòng một bản ghi) → store_end(w, true) đổi tên nguyên tử như
// store_write_atomic; store_end(w, false) bỏ file tạm, giữ nguyên file cũ.
typedef struct store_writer store_writer_t;
store_writer_t *store_begin(const char *path);                      // NULL nếu không mở được hoặc đang khôi phục
bool            store_put_line(store_writer_t *w, const char *line);
esp_err_t       store_end(store_writer_t *w, bool commit);
// Đọc: store_open_lines (NULL nếu chưa có file) → store_next_line trả độ dài dòng (đã bỏ \r\n), 0 khi hết file,
// -1 khi dòng dài hơn cap (dòng đó bị bỏ qua) → store_close_lines.
typedef struct store_reader store_reader_t;
store_reader_t *store_open_lines(const char *path);
int             store_next_line(store_reader_t *r, char *buf, size_t cap);
void            store_close_lines(store_reader_t *r);
// Hẹn ghi: gọi fn sau 1 giây kể từ lần hẹn cuối cùng với cùng fn (gom nhiều thay đổi liên tiếp thành một lần ghi).
// fn chạy trên task ghi riêng (stack 8 KB), không chạy trong handler hay callback esp_timer. Gọi được từ mọi task.
typedef void (*store_save_fn)(void);
void      store_save_later(store_save_fn fn);

// ---------- thời gian (UTC+7)
bool    time_is_synced(void);
int32_t time_today(void);       // số ngày từ 1970-01-01 theo giờ Việt Nam; -1 nếu chưa có giờ

// ---------- khởi động (chỉ app_main gọi)
void platform_init(void);       // NVS, LittleFS, tài khoản, Wi-Fi, SNTP, đăng ký lệnh auth.* và system.*
void platform_start(void);      // mở TCP server; gọi sau khi mọi module đã đăng ký lệnh

#ifdef __cplusplus
}
#endif
