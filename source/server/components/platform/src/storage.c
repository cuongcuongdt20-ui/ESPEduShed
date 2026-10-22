// Lưu trữ trên LittleFS gắn ở /data (TV1).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_littlefs.h"
#include "esp_log.h"
#include "platform.h"
#include "platform_internal.h"

static const char *TAG = "storage";

esp_err_t storage_init(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/data",
        .partition_label = "data",
        .format_if_mount_failed = true,       // lần đầu chạy: tự format
        .dont_mount = false,
    };
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "khong mount duoc /data: %s", esp_err_to_name(err));
        return err;
    }
    size_t total = 0, used = 0;
    storage_info(&total, &used);
    ESP_LOGI(TAG, "/data: da dung %u/%u byte", (unsigned) used, (unsigned) total);
    return ESP_OK;
}

bool storage_info(size_t *total, size_t *used)
{
    return esp_littlefs_info("data", total, used) == ESP_OK;
}

esp_err_t store_write_atomic(const char *path, const char *data, size_t len)
{
    char tmp[64];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int) sizeof(tmp)) return ESP_ERR_INVALID_ARG;
    FILE *f = fopen(tmp, "wb");
    if (!f) return ESP_FAIL;
    size_t w = fwrite(data, 1, len, f);
    int flushed = fflush(f);
    fsync(fileno(f));
    fclose(f);
    if (w != len || flushed != 0) {
        remove(tmp);
        ESP_LOGE(TAG, "ghi %s that bai (het cho?)", path);
        return ESP_ERR_NO_MEM;
    }
    // Bước quyết định: đổi tên ghi đè file cũ một cách nguyên tử trên LittleFS.
    // Mất điện trước bước này: còn nguyên bản cũ. Sau bước này: đã là bản mới. Không có trạng thái nửa vời.
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
