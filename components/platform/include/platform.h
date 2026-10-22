// platform.h: hợp đồng của TV1 (PHẦN A5 của prompt). Không đổi chữ ký hàm khi chưa được cả nhóm duyệt.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------- khởi tạo (chỉ app_main gọi)
void           platform_init(void);
httpd_handle_t platform_http(void);
void           platform_register_routes(httpd_handle_t http);
void           platform_register_static(httpd_handle_t http);   // gọi cuối cùng

// ---------- lưu trữ (LittleFS, dữ liệu ở /data)
esp_err_t store_write_atomic(const char *path, const char *data, size_t len);  // ghi file tạm rồi đổi tên
char     *store_read_all(const char *path, size_t *len);                        // caller free(); NULL nếu chưa có

// ---------- bảo vệ thao tác ghi
bool auth_check_write(httpd_req_t *req);       // false thì đã tự trả 401; gọi đầu mọi handler thêm/sửa/xoá

// ---------- thông báo thời gian thực
void ws_notify_changed(const char *entity, uint32_t id);   // an toàn khi gọi từ handler HTTP hoặc task khác

// ---------- thời gian (UTC+7)
bool    time_is_synced(void);
int32_t time_today(void);                      // số ngày từ 1970-01-01 theo giờ VN; -1 nếu chưa có giờ

// ---------- JSON và HTTP dùng chung
cJSON    *json_read_body(httpd_req_t *req, size_t max_len);  // NULL thì đã tự trả lỗi
esp_err_t json_reply(httpd_req_t *req, int status, cJSON *body);  // giải phóng body; body NULL = không có nội dung
esp_err_t json_error(httpd_req_t *req, int status, const char *code, const char *msg, const char *field);

// Lấy tham số query ?key=value; false nếu không có
bool http_query(httpd_req_t *req, const char *key, char *out, size_t out_len);
// Tách id sau tiền tố: uri "/api/schools/3/periods", prefix "/api/schools/" → id = 3, *rest = "/periods"
bool http_path_id(httpd_req_t *req, const char *prefix, uint32_t *id, const char **rest);

// Đăng ký nhanh một route (dùng cho mọi module)
esp_err_t http_route(httpd_handle_t http, const char *uri, httpd_method_t method,
                     esp_err_t (*handler)(httpd_req_t *));

#define LD_BODY_MAX 8192   // giới hạn kích thước body mặc định

#ifdef __cplusplus
}
#endif
