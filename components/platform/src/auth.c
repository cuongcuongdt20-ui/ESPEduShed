// Đăng nhập và bảo vệ thao tác ghi (TV1).
// BẢN GIẢ: mọi thao tác ghi đều được phép, đăng nhập luôn thành công với token "dev".
#include <string.h>
#include "esp_log.h"
#include "platform.h"
#include "platform_internal.h"

void auth_init(void)
{
    // TODO(TV1): đọc mật khẩu băm (PBKDF2-SHA256 + salt) từ NVS; chưa có thì tạo tài khoản mặc định
    //            và bắt đổi mật khẩu ở lần đăng nhập đầu
}

bool auth_check_write(httpd_req_t *req)
{
    // TODO(TV1): đọc header "Authorization: Bearer <token>", kiểm tra phiên còn hạn;
    //            sai thì json_error(req, 401, "UNAUTHORIZED", ...) rồi trả false
    (void) req;
    return true;
}

// POST /api/auth/login {"password":"..."} → {"token":"...","expiresAt":"YYYY-MM-DD"}
static esp_err_t login(httpd_req_t *req)
{
    cJSON *body = json_read_body(req, 512);
    if (!body) return ESP_OK;
    // TODO(TV1): kiểm tra mật khẩu, giới hạn 5 lần sai / 60 giây, sinh token 128 bit, tối đa 8 phiên
    cJSON_Delete(body);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "token", "dev");
    return json_reply(req, 200, r);
}

// POST /api/auth/logout
static esp_err_t logout(httpd_req_t *req)
{
    // TODO(TV1): xoá phiên của token hiện tại
    return json_reply(req, 204, NULL);
}

// POST /api/auth/password {"old":"...","new":"..."}
static esp_err_t change_password(httpd_req_t *req)
{
    // TODO(TV1)
    return json_error(req, 501, "NOT_IMPLEMENTED", "Chua lam", NULL);
}

void auth_register(httpd_handle_t http)
{
    http_route(http, "/api/auth/login", HTTP_POST, login);
    http_route(http, "/api/auth/logout", HTTP_POST, logout);
    http_route(http, "/api/auth/password", HTTP_POST, change_password);
}
