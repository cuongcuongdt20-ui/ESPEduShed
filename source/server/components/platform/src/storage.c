// Lưu trữ trên LittleFS gắn ở /data (TV1).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_littlefs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ld_lock.h"
#include "platform.h"
#include "platform_internal.h"

static const char *TAG = "storage";

// ---------- Task ghi trễ (store_save_later) ----------
// Ghi flash và dựng JSON tốn stack và chặn lâu, nên không làm trong callback esp_timer (stack 3,5 KB, dùng chung với
// các timer khác) mà dồn về một task riêng. Mỗi module đăng ký hàm ghi của mình; hẹn lại thì dời hạn thêm 1 giây.
#define SAVE_SLOTS    8
#define SAVE_DELAY_MS 1000

typedef struct {
    store_save_fn fn;
    int64_t       due_ms;        // 0: không có hẹn
} save_slot_t;

static save_slot_t s_saves[SAVE_SLOTS];
static ld_lock_t   s_save_lock;
static TaskHandle_t s_save_task;

void store_save_later(store_save_fn fn)
{
    if (!fn || !s_save_lock) return;
    ld_lock(s_save_lock);
    int free_i = -1, i = 0;
    for (; i < SAVE_SLOTS && s_saves[i].fn != fn; i++)
        if (!s_saves[i].fn && free_i < 0) free_i = i;
    if (i == SAVE_SLOTS) i = free_i;
    if (i >= 0) {
        s_saves[i].fn = fn;
        s_saves[i].due_ms = ld_now_ms() + SAVE_DELAY_MS;
    }
    ld_unlock(s_save_lock);
    if (i < 0) {
        ESP_LOGE(TAG, "het cho hen ghi, ghi ngay");
        fn();
    }
}

static void save_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
        for (int i = 0; i < SAVE_SLOTS; i++) {
            store_save_fn run = NULL;
            ld_lock(s_save_lock);
            if (s_saves[i].fn && s_saves[i].due_ms && ld_now_ms() >= s_saves[i].due_ms) {
                run = s_saves[i].fn;
                s_saves[i].due_ms = 0;               // giữ fn để lần hẹn sau dùng lại ô
            }
            ld_unlock(s_save_lock);
            if (run) run();                           // chạy ngoài khoá: hàm ghi tự lấy khoá của module nó
        }
    }
}

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
    ld_lock_init(&s_save_lock);
    xTaskCreate(save_task, "store_save", 8192, NULL, 3, &s_save_task);
    return ESP_OK;
}

bool storage_info(size_t *total, size_t *used)
{
    return esp_littlefs_info("data", total, used) == ESP_OK;
}

// Sau khi system.restore đã thay một file, dữ liệu trong RAM của các module là bản cũ: chặn mọi lần ghi tới khi
// khởi động lại, để bản cũ không đè lên file vừa khôi phục.
static volatile bool s_writes_frozen;

bool store_freeze_writes(void)
{
    bool was_frozen = s_writes_frozen;
    s_writes_frozen = true;
    return was_frozen;
}

void store_unfreeze_writes(void)
{
    s_writes_frozen = false;
}

esp_err_t store_write_atomic(const char *path, const char *data, size_t len)
{
    if (s_writes_frozen) {
        ESP_LOGW(TAG, "dang khoi phuc du lieu, bo qua ghi %s (thay doi se mat khi khoi dong lai)", path);
        return ESP_ERR_INVALID_STATE;
    }
    char tmp[64];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int) sizeof(tmp)) return ESP_ERR_INVALID_ARG;
    FILE *f = fopen(tmp, "wb");
    if (!f) return ESP_FAIL;
    size_t w = fwrite(data, 1, len, f);
    bool synced = fflush(f) == 0 && fsync(fileno(f)) == 0;
    bool closed = fclose(f) == 0;
    if (w != len || !synced || !closed) {
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

// ---------- Ghi / đọc theo dòng ----------

struct store_writer {
    FILE *f;
    bool  ok;
    char  path[48];
    char  tmp[56];
};

store_writer_t *store_begin(const char *path)
{
    if (s_writes_frozen) {
        ESP_LOGW(TAG, "dang khoi phuc du lieu, bo qua ghi %s (thay doi se mat khi khoi dong lai)", path);
        return NULL;
    }
    store_writer_t *w = calloc(1, sizeof(*w));
    if (!w) return NULL;
    if (snprintf(w->path, sizeof(w->path), "%s", path) >= (int) sizeof(w->path)
        || snprintf(w->tmp, sizeof(w->tmp), "%s.tmp", path) >= (int) sizeof(w->tmp)
        || !(w->f = fopen(w->tmp, "wb"))) {
        free(w);
        return NULL;
    }
    w->ok = true;
    return w;
}

bool store_put_line(store_writer_t *w, const char *line)
{
    if (!w || !w->ok) return false;
    size_t n = strlen(line);
    w->ok = fwrite(line, 1, n, w->f) == n && fputc('\n', w->f) != EOF;
    return w->ok;
}

esp_err_t store_end(store_writer_t *w, bool commit)
{
    if (!w) return ESP_FAIL;
    bool ok = w->ok && commit && fflush(w->f) == 0 && fsync(fileno(w->f)) == 0;
    if (fclose(w->f) != 0) ok = false;
    // Như store_write_atomic: mất điện trước rename còn bản cũ, sau rename là bản mới
    if (ok && rename(w->tmp, w->path) != 0) ok = false;
    if (!ok) remove(w->tmp);
    esp_err_t r = ok ? ESP_OK : (commit ? ESP_ERR_NO_MEM : ESP_FAIL);
    if (commit && !ok) ESP_LOGE(TAG, "ghi %s that bai (het cho?)", w->path);
    free(w);
    return r;
}

struct store_reader {
    FILE *f;
};

store_reader_t *store_open_lines(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    store_reader_t *r = malloc(sizeof(*r));
    if (!r) {
        fclose(f);
        return NULL;
    }
    r->f = f;
    return r;
}

int store_next_line(store_reader_t *r, char *buf, size_t cap)
{
    if (!r || !fgets(buf, (int) cap, r->f)) return 0;
    size_t n = strlen(buf);
    if (n > 0 && buf[n - 1] != '\n' && !feof(r->f)) {
        // dòng dài hơn bộ đệm: bỏ phần còn lại của dòng
        int c;
        while ((c = fgetc(r->f)) != EOF && c != '\n') {
        }
        return -1;
    }
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
    return n == 0 ? store_next_line(r, buf, cap) : (int) n;    // bỏ dòng trống
}

void store_close_lines(store_reader_t *r)
{
    if (!r) return;
    fclose(r->f);
    free(r);
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
