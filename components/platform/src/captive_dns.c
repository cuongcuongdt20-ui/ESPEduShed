// DNS server nhỏ cho captive portal ở chế độ AP (TV1).
// Mọi truy vấn DNS đều trả về 192.168.4.1 để điện thoại tự mở trang cài đặt.
#include "esp_log.h"
#include "platform_internal.h"

void captive_dns_start(void)
{
    // TODO(TV1): tạo task, mở socket UDP cổng 53, đọc truy vấn, trả bản ghi A = 192.168.4.1
    ESP_LOGW("dns", "captive DNS chua lam");
}

void captive_dns_stop(void)
{
    // TODO(TV1): dừng task, đóng socket
}
