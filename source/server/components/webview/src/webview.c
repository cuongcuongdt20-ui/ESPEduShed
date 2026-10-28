// Trang web chỉ đọc xem lịch trên điện thoại (đặc tả trong webview.h).
//
// Chạy trên task của esp_http_server, tách khỏi task TCP: calendar_range_json tự khoá dữ liệu lịch, danh mục và
// xung đột nên gọi được từ đây. Khoá bí mật 128 bit nên không cần chặn dò như đăng nhập TCP; so sánh thời gian
// hằng để không lộ khoá qua thời gian phản hồi.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "conflict.h"
#include "dt.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "ld_lock.h"
#include "nvs.h"
#include "platform.h"
#include "schedule.h"
#include "webview.h"

static const char *TAG = "web";

#define KEY_LEN     32                       // 16 byte ngẫu nhiên dạng hex
#define WEB_BUDGET  (48 * 1024)              // byte JSON tối đa của items: HTTP không bị giới hạn khung 8 KB như TCP

extern const char s_html_start[] asm("_binary_calendar_html_start");
extern const char s_html_end[] asm("_binary_calendar_html_end");
// FullCalendar 6.1.15 (bản global + locale vi, giấy phép MIT) nén gzip, nhúng trong firmware thay vì tải từ CDN:
// trang chứa khoá bí mật không chạy script của bên thứ ba, và xem được cả khi điện thoại không có Internet.
extern const char s_fc_start[] asm("_binary_fullcalendar_js_gz_start");
extern const char s_fc_end[] asm("_binary_fullcalendar_js_gz_end");

static ld_lock_t s_lock;
static char      s_key[KEY_LEN + 1];

static void new_key(char out[KEY_LEN + 1])
{
    uint8_t rnd[KEY_LEN / 2];
    ld_random(rnd, sizeof(rnd));
    ld_hex_encode(rnd, sizeof(rnd), out);
}

static bool save_key(const char *key)
{
    nvs_handle_t h;
    if (nvs_open("web", NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = nvs_set_str(h, "key", key) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

static void load_key(void)
{
    nvs_handle_t h;
    size_t len = sizeof(s_key);
    bool ok = false;
    if (nvs_open("web", NVS_READONLY, &h) == ESP_OK) {
        ok = nvs_get_str(h, "key", s_key, &len) == ESP_OK && strlen(s_key) == KEY_LEN;
        nvs_close(h);
    }
    if (!ok) {
        new_key(s_key);
        save_key(s_key);
    }
}

// p trỏ vào khoá trong URI, kết thúc ở '/', '?' hoặc hết chuỗi
static bool key_matches(const char *p)
{
    size_t n = strcspn(p, "/?");
    if (n != KEY_LEN) return false;
    uint8_t diff = 0;
    ld_lock(s_lock);
    for (size_t i = 0; i < KEY_LEN; i++) diff |= (uint8_t) (p[i] ^ s_key[i]);
    ld_unlock(s_lock);
    return diff == 0;
}

static void common_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Referrer-Policy", "no-referrer");      // link chứa khoá không lộ qua CDN
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
}

static esp_err_t not_found(httpd_req_t *req)
{
    common_headers(req);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Khong tim thay");
}

static esp_err_t send_json_error(httpd_req_t *req, const char *status, const char *msg)
{
    char body[96];
    snprintf(body, sizeof(body), "{\"error\":\"%s\"}", msg);
    common_headers(req);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, body);
}

// GET /c/<key>
static esp_err_t page_get(httpd_req_t *req)
{
    if (!key_matches(req->uri + 3)) return not_found(req);
    common_headers(req);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, s_html_start, s_html_end - s_html_start - 1);   // EMBED_TXTFILES thêm '\0' ở cuối
}

// GET /api/<key>/range?from=YYYY-MM-DD&to=YYYY-MM-DD
static esp_err_t api_get(httpd_req_t *req)
{
    const char *p = req->uri + 5;
    if (!key_matches(p) || strncmp(p + KEY_LEN, "/range", 6) != 0) return not_found(req);
    char end = p[KEY_LEN + 6];
    if (end != '\0' && end != '?') return not_found(req);               // đúng /range, không nhận /rangeXyz

    char query[64], a[12], b[12], msg[64];
    int32_t from, to;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK
        || httpd_query_key_value(query, "from", a, sizeof(a)) != ESP_OK
        || httpd_query_key_value(query, "to", b, sizeof(b)) != ESP_OK || !dt_parse_date(a, &from)
        || !dt_parse_date(b, &to) || to < from || to - from + 1 > SCHED_MAX_RANGE_DAYS) {
        snprintf(msg, sizeof(msg), "from, to dang YYYY-MM-DD, toi da %d ngay", SCHED_MAX_RANGE_DAYS);
        return send_json_error(req, "400 Bad Request", msg);
    }

    cJSON *out = cJSON_CreateObject();
    if (!out) return send_json_error(req, "500 Internal Server Error", "Het bo nho");
    int r = calendar_range_json(LD_OWNER_ID, from, to, WEB_BUDGET, out);
    char *text = r == LD_OK ? cJSON_PrintUnformatted(out) : NULL;
    cJSON_Delete(out);
    // BAD_ARG: sched_expand không chứa hết số buổi trong khoảng ngày, xem khoảng ngắn hơn (theo tuần) thì được
    if (r == LD_ERR_BAD_ARG)
        return send_json_error(req, "400 Bad Request", "Khoang ngay co qua nhieu buoi, hay xem theo tuan");
    if (!text) return send_json_error(req, "500 Internal Server Error", "Khong doc duoc lich");
    common_headers(req);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, text);
    cJSON_free(text);
    return err;
}

// GET /fc.js: thư viện lịch, công khai (không chứa dữ liệu), cho phép trình duyệt giữ lại 1 ngày
static esp_err_t fc_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    return httpd_resp_send(req, s_fc_start, s_fc_end - s_fc_start);
}

static void link_json(cJSON *out)
{
    ld_lock(s_lock);
    cJSON_AddStringToObject(out, "key", s_key);
    ld_unlock(s_lock);
    cJSON_AddNumberToObject(out, "port", CONFIG_LD_HTTP_PORT);
}

// web.link → {key, port}: app ghép thành http://<địa chỉ>:<port>/c/<key>
static int cmd_link(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    link_json(out);
    return LD_OK;
}

// web.rotate → {key, port}: sinh khoá mới, link cũ hết hiệu lực ngay
static int cmd_rotate(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    char key[KEY_LEN + 1];
    new_key(key);
    if (!save_key(key)) return ld_fail(out, LD_ERR_INTERNAL, "Khong luu duoc khoa moi", NULL);
    ld_lock(s_lock);
    strlcpy(s_key, key, sizeof(s_key));
    ld_unlock(s_lock);
    link_json(out);
    ESP_LOGI(TAG, "da doi link xem lich");
    return LD_OK;
}

void webview_init(void)
{
    ld_lock_init(&s_lock);
    load_key();
    ld_register_cmd("web.link", cmd_link, LD_ROLE_USER);
    ld_register_cmd("web.rotate", cmd_rotate, LD_ROLE_USER);
}

void webview_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = CONFIG_LD_HTTP_PORT;
    cfg.ctrl_port = 32769;                   // tránh trùng cổng điều khiển mặc định nếu có httpd khác
    cfg.stack_size = 8192;                   // cJSON và sinh buổi cần stack lớn hơn mặc định 4 KB
    cfg.send_wait_timeout = 15;              // gửi fc.js (80 KB) qua mạng di động chậm
    cfg.max_open_sockets = 4;
    cfg.lru_purge_enable = true;             // hết chỗ thì đóng kết nối cũ nhất thay vì từ chối
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "khong mo duoc HTTP cong %d", CONFIG_LD_HTTP_PORT);
        return;
    }
    static const httpd_uri_t page = {.uri = "/c/*", .method = HTTP_GET, .handler = page_get};
    static const httpd_uri_t api = {.uri = "/api/*", .method = HTTP_GET, .handler = api_get};
    static const httpd_uri_t fc = {.uri = "/fc.js", .method = HTTP_GET, .handler = fc_get};
    httpd_register_uri_handler(server, &page);
    httpd_register_uri_handler(server, &api);
    httpd_register_uri_handler(server, &fc);
    // Không in khoá ra log: ai đọc được log serial sẽ có link xem lịch. Lấy link trong app (web.link).
    ESP_LOGI(TAG, "web xem lich o cong %d, lay link trong app (muc Truy cap tu xa)", CONFIG_LD_HTTP_PORT);
}
