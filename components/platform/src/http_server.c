// Khởi động esp_http_server (TV1).
#include "esp_log.h"
#include "platform.h"
#include "platform_internal.h"

static httpd_handle_t s_http;

esp_err_t http_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 10;          // phải <= CONFIG_LWIP_MAX_SOCKETS - 3
    cfg.max_uri_handlers = 80;          // đủ cho route của cả 4 module
    cfg.stack_size = 8192;              // handler dùng cJSON cần stack lớn
    cfg.lru_purge_enable = true;        // hết socket thì đóng kết nối cũ nhất
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    // TODO(TV1): bật TCP keep-alive để phát hiện client WebSocket chết (các trường keep_alive_* của IDF 5.x)
    esp_err_t err = httpd_start(&s_http, &cfg);
    if (err == ESP_OK) ESP_LOGI("http", "HTTP server cong %d", cfg.server_port);
    return err;
}

httpd_handle_t platform_http(void)
{
    return s_http;
}
