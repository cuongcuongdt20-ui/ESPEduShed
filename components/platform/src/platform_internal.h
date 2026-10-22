// Hàm nội bộ của TV1, các module khác không dùng.
#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t storage_mount(void);         // /www (giao diện, chỉ đọc) và /data (dữ liệu)
void      wifi_start(void);            // không chặn: HTTP server chạy được kể cả khi chưa có Wi-Fi
void      captive_dns_start(void);
void      captive_dns_stop(void);
void      time_start(void);
void      mdns_start(void);
esp_err_t http_start(void);
void      auth_init(void);

void ws_register(httpd_handle_t http);
void auth_register(httpd_handle_t http);
void system_register(httpd_handle_t http);
void static_register(httpd_handle_t http);
