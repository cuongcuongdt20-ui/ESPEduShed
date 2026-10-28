// Lệnh hệ thống (TV1): system.ping, system.status, system.backup, system.restore (system.ddns ở ddns.c).
//
// Sao lưu và khôi phục đi theo từng mẩu 4 KB (mã hoá base64), vì mỗi khung TCP tối đa 8 KB:
//   system.backup {}                         → {files:[{name, size}]}
//   system.backup {file, offset}             → {file, offset, size, data, eof}
//   system.restore {file, offset, data, last, reboot}
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "mbedtls/base64.h"
#include "platform.h"
#include "platform_internal.h"
#include "session.h"

#define CHUNK_RAW 4096
static const char *TAG = "system";

// Tên file sao lưu hợp lệ: chữ thường, số, gạch dưới, đuôi .json (không cho "../" hay thư mục con)
static bool valid_file(const char *name)
{
    size_t n = name ? strlen(name) : 0;
    if (n < 6 || n > 32 || strcmp(name + n - 5, ".json") != 0) return false;
    for (size_t i = 0; i < n - 5; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

// system.ping → {pong}
static int cmd_ping(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    cJSON_AddNumberToObject(out, "pong", (double) ld_now_ms());
    return LD_OK;
}

// system.status → thông tin để giám sát và đo thực nghiệm
static int cmd_status(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    uint32_t requests, errors, max_us;
    protocol_stats(&requests, &errors, &max_us);
    char ip[16];
    wifi_ip_str(ip);
    int8_t rssi;
    size_t total = 0, used = 0;

    cJSON_AddStringToObject(out, "name", "espedushed");
    cJSON_AddStringToObject(out, "fw", LD_FW_VERSION);
    cJSON_AddStringToObject(out, "mode", wifi_is_ap() ? "ap" : "sta");
    cJSON_AddStringToObject(out, "ip", ip);
    if (wifi_rssi(&rssi)) cJSON_AddNumberToObject(out, "rssi", rssi);
    cJSON_AddNumberToObject(out, "uptimeS", (double) (ld_now_ms() / 1000));
    cJSON_AddBoolToObject(out, "timeSynced", time_is_synced());
    cJSON_AddNumberToObject(out, "clients", tcp_client_count());
    cJSON_AddNumberToObject(out, "maxClients", LD_MAX_CLIENTS);
    cJSON_AddNumberToObject(out, "sessions", session_count(ld_now_ms()));
    cJSON_AddNumberToObject(out, "heapFree", esp_get_free_heap_size());
    cJSON_AddNumberToObject(out, "heapMin", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(out, "heapLargest", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(out, "requests", requests);
    cJSON_AddNumberToObject(out, "errors", errors);
    cJSON_AddNumberToObject(out, "maxHandlerUs", max_us);
    if (storage_info(&total, &used)) {
        cJSON_AddNumberToObject(out, "dataTotal", total);
        cJSON_AddNumberToObject(out, "dataUsed", used);
    }
    if (ctx->user_id) ddns_status_json(out);       // system.status là lệnh công khai: chỉ phiên đã đăng nhập mới thấy
    return LD_OK;
}

static int list_files(cJSON *out)
{
    DIR *dir = opendir("/data");
    if (!dir) return ld_fail(out, LD_ERR_INTERNAL, "Khong mo duoc /data", NULL);
    cJSON *arr = cJSON_AddArrayToObject(out, "files");
    struct dirent *e;
    while ((e = readdir(dir)) != NULL) {
        if (!valid_file(e->d_name)) continue;             // bỏ .tmp, .restore và file lạ
        char path[8 + sizeof(e->d_name)];
        struct stat st;
        snprintf(path, sizeof(path), "/data/%s", e->d_name);
        if (stat(path, &st) != 0) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", e->d_name);
        cJSON_AddNumberToObject(o, "size", (double) st.st_size);
        cJSON_AddItemToArray(arr, o);
    }
    closedir(dir);
    return LD_OK;
}

// system.backup (admin)
static int cmd_backup(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    const char *file = ld_get_str(data, "file");
    if (!file) return list_files(out);
    if (!valid_file(file)) return ld_fail(out, LD_ERR_BAD_ARG, "Ten file khong hop le", "file");
    int offset = 0;
    ld_get_int(data, "offset", &offset);

    char path[48];
    snprintf(path, sizeof(path), "/data/%s", file);
    FILE *f = fopen(path, "rb");
    if (!f) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co file nay", "file");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (offset < 0 || offset > size) {
        fclose(f);
        return ld_fail(out, LD_ERR_BAD_ARG, "offset vuot qua kich thuoc file", "offset");
    }
    fseek(f, offset, SEEK_SET);
    static uint8_t raw[CHUNK_RAW];
    static unsigned char b64[CHUNK_RAW * 4 / 3 + 8];
    size_t n = fread(raw, 1, sizeof(raw), f);
    fclose(f);
    size_t olen = 0;
    mbedtls_base64_encode(b64, sizeof(b64), &olen, raw, n);
    b64[olen] = 0;

    cJSON_AddStringToObject(out, "file", file);
    cJSON_AddNumberToObject(out, "offset", offset);
    cJSON_AddNumberToObject(out, "size", (double) size);
    cJSON_AddStringToObject(out, "data", (const char *) b64);
    cJSON_AddBoolToObject(out, "eof", offset + (long) n >= size);
    return LD_OK;
}

static void restart_cb(void *arg)
{
    esp_restart();
}

// system.restore (admin): ghi từng mẩu vào /data/<file>.restore; mẩu cuối thì đổi tên đè file thật.
// Khôi phục xong phải khởi động lại ("reboot": true ở mẩu cuối của file cuối) để mọi module nạp lại dữ liệu.
static int cmd_restore(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    const char *file = ld_get_str(data, "file");
    const char *b64 = ld_get_str(data, "data");
    int offset = -1;
    bool last = false, reboot = false;
    if (!valid_file(file)) return ld_fail(out, LD_ERR_BAD_ARG, "Ten file khong hop le", "file");
    if (!b64) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu data", "data");
    if (!ld_get_int(data, "offset", &offset) || offset < 0) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu offset", "offset");
    ld_get_bool(data, "last", &last);
    ld_get_bool(data, "reboot", &reboot);

    static uint8_t raw[CHUNK_RAW];
    size_t n = 0;
    if (mbedtls_base64_decode(raw, sizeof(raw), &n, (const unsigned char *) b64, strlen(b64)) != 0)
        return ld_fail(out, LD_ERR_BAD_ARG, "data khong phai base64 hoac qua 4 KB", "data");

    char tmp[56];
    snprintf(tmp, sizeof(tmp), "/data/%s.restore", file);
    struct stat st;
    long have = stat(tmp, &st) == 0 ? (long) st.st_size : 0;
    if (offset != 0 && offset != have)
        return ld_fail(out, LD_ERR_BAD_ARG, "offset khong khop phan da nhan", "offset");
    FILE *f = fopen(tmp, offset == 0 ? "wb" : "ab");
    if (!f) return ld_fail(out, LD_ERR_INTERNAL, "Khong mo duoc file tam", NULL);
    size_t w = fwrite(raw, 1, n, f);
    fclose(f);
    if (w != n) return ld_fail(out, LD_ERR_FULL, "Het cho luu", NULL);

    if (last) {
        char path[48];
        snprintf(path, sizeof(path), "/data/%s", file);
        store_freeze_writes();               // từ đây dữ liệu trong RAM đã cũ: không cho ghi đè file vừa khôi phục
        if (rename(tmp, path) != 0) return ld_fail(out, LD_ERR_INTERNAL, "Khong thay duoc file", NULL);
        ESP_LOGW(TAG, "da khoi phuc %s, can khoi dong lai de nap", file);
    }
    cJSON_AddNumberToObject(out, "written", (double) (offset + (long) n));
    if (last && reboot) {
        esp_timer_handle_t t;
        esp_timer_create_args_t args = {.callback = restart_cb, .name = "restore_reboot"};
        if (esp_timer_create(&args, &t) == ESP_OK) esp_timer_start_once(t, 1500 * 1000);
        cJSON_AddBoolToObject(out, "restarting", true);
    }
    return LD_OK;
}

void system_register_cmds(void)
{
    ld_register_cmd("system.ping", cmd_ping, LD_ROLE_NONE);
    ld_register_cmd("system.status", cmd_status, LD_ROLE_NONE);
    ld_register_cmd("system.backup", cmd_backup, LD_ROLE_USER);
    ld_register_cmd("system.restore", cmd_restore, LD_ROLE_USER);
}
