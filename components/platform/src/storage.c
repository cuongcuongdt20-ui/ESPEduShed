// Lưu trữ trên LittleFS (TV1): /www là giao diện (nạp từ thư mục web/), /data là dữ liệu người dùng.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_littlefs.h"
#include "esp_log.h"
#include "platform.h"
#include "platform_internal.h"

static const char *TAG = "storage";

static esp_err_t mount(const char *base, const char *label, bool format)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = base,
        .partition_label = label,
        .format_if_mount_failed = format,
        .dont_mount = false,
    };
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount %s (%s) loi: %s", base, label, esp_err_to_name(err));
        return err;
    }
    size_t total = 0, used = 0;
    esp_littlefs_info(label, &total, &used);
    ESP_LOGI(TAG, "%s: %u/%u byte", base, (unsigned) used, (unsigned) total);
    return ESP_OK;
}

esp_err_t storage_mount(void)
{
    esp_err_t www = mount("/www", "www", false);    // ảnh do idf.py flash tạo, không tự format
    esp_err_t data = mount("/data", "data", true);  // lần đầu chạy: tự format
    return www == ESP_OK && data == ESP_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t store_write_atomic(const char *path, const char *data, size_t len)
{
    char tmp[64];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int) sizeof(tmp)) return ESP_ERR_INVALID_ARG;
    FILE *f = fopen(tmp, "wb");
    if (!f) return ESP_FAIL;
    size_t w = fwrite(data, 1, len, f);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    if (w != len) {
        remove(tmp);
        return ESP_ERR_NO_MEM;          // thường là hết chỗ trên phân vùng
    }
    // LittleFS đổi tên ghi đè file cũ một cách nguyên tử: mất điện giữa chừng thì còn bản cũ hoặc bản mới
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return ESP_FAIL;
    }
    return ESP_OK;
}

char *store_read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t) size + 1);
    if (buf && fread(buf, 1, (size_t) size, f) == (size_t) size) {
        buf[size] = 0;
        if (len) *len = (size_t) size;
    } else {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}
