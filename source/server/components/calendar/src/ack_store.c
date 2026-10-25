// Lưu các xung đột đã xác nhận xuống /data/conflict_ack.json (TV4).
// Định dạng: {"schema":1,"acks":[{"a":1,"b":2,"date":"2026-09-02"}]}
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "platform.h"
#include "calendar_internal.h"
#include "dt.h"

#define ACK_FILE "/data/conflict_ack.json"
#define KEEP_DAYS 14           // xác nhận của ngày đã qua quá 14 ngày thì bỏ

static const char *TAG = "calendar";
static esp_timer_handle_t s_timer;

void calendar_acks_load(void)
{
    size_t len = 0;
    char *text = store_read_all(ACK_FILE, &len);
    if (!text) return;
    cJSON *root = cJSON_ParseWithLength(text, len);
    free(text);
    const cJSON *arr = cJSON_GetObjectItem(root, "acks");
    ld_lock(g_cal_lock);
    g_n_acks = 0;
    const cJSON *it;
    cJSON_ArrayForEach(it, arr)
    {
        const cJSON *a = cJSON_GetObjectItem(it, "a"), *b = cJSON_GetObjectItem(it, "b");
        const cJSON *d = cJSON_GetObjectItem(it, "date");
        int32_t day;
        if (!cJSON_IsNumber(a) || !cJSON_IsNumber(b) || !cJSON_IsString(d) || !dt_parse_date(d->valuestring, &day)) continue;
        if (g_n_acks >= CAL_MAX_ACKS) break;
        g_acks[g_n_acks].source_a = (uint32_t) a->valuedouble;
        g_acks[g_n_acks].source_b = (uint32_t) b->valuedouble;
        g_acks[g_n_acks].day = day;
        g_n_acks++;
    }
    ld_unlock(g_cal_lock);
    cJSON_Delete(root);
    ESP_LOGI(TAG, "nap %d xac nhan xung dot", g_n_acks);
}

// Bỏ xác nhận của ngày đã qua lâu (chỉ khi ESP đã có giờ) để file không phình. Gọi khi đang giữ khoá.
static void purge_old(void)
{
    int32_t today = time_today();
    if (today < 0) return;
    int k = 0;
    for (int i = 0; i < g_n_acks; i++)
        if (g_acks[i].day >= today - KEEP_DAYS) g_acks[k++] = g_acks[i];
    g_n_acks = k;
}

static void save_now(void *arg)
{
    (void) arg;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "schema", 1);
    cJSON *arr = cJSON_AddArrayToObject(root, "acks");
    ld_lock(g_cal_lock);
    purge_old();
    for (int i = 0; i < g_n_acks; i++) {
        char d[11];
        dt_format_date(g_acks[i].day, d);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "a", g_acks[i].source_a);
        cJSON_AddNumberToObject(o, "b", g_acks[i].source_b);
        cJSON_AddStringToObject(o, "date", d);
        cJSON_AddItemToArray(arr, o);
    }
    ld_unlock(g_cal_lock);
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (text) {
        if (store_write_atomic(ACK_FILE, text, strlen(text)) != ESP_OK) ESP_LOGE(TAG, "khong ghi duoc %s", ACK_FILE);
        free(text);
    }
}

// Gom thay đổi: ghi flash sau 1 giây kể từ thay đổi cuối (README A8)
void calendar_acks_mark_dirty(void)
{
    if (!s_timer) {
        esp_timer_create_args_t args = {.callback = save_now, .name = "calendar_ack_save"};
        esp_timer_create(&args, &s_timer);
    }
    esp_timer_stop(s_timer);
    esp_timer_start_once(s_timer, 1000 * 1000);
}

bool calendar_ack_add(uint32_t a, uint32_t b, int32_t day)
{
    if (a > b) {
        uint32_t t = a;
        a = b;
        b = t;
    }
    bool ok = true;
    ld_lock(g_cal_lock);
    bool exists = false;
    for (int i = 0; i < g_n_acks && !exists; i++)
        exists = g_acks[i].source_a == a && g_acks[i].source_b == b && g_acks[i].day == day;
    if (!exists) {
        if (g_n_acks >= CAL_MAX_ACKS) purge_old();
        if (g_n_acks >= CAL_MAX_ACKS) {
            ok = false;
        } else {
            g_acks[g_n_acks].source_a = a;
            g_acks[g_n_acks].source_b = b;
            g_acks[g_n_acks].day = day;
            g_n_acks++;
        }
    }
    ld_unlock(g_cal_lock);
    if (ok && !exists) calendar_acks_mark_dirty();
    return ok;
}

void calendar_init(void)
{
    ld_lock_init(&g_cal_lock);
    calendar_acks_load();
    calendar_register_cmds();
}
