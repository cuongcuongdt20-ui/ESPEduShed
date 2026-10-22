// Lưu lịch xuống /data/schedule.json (TV3).
// Định dạng: {"schema":1,"nextId":..,"series":[...],"tasks":[...],"exceptions":[...]}
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "platform.h"
#include "schedule.h"
#include "schedule_internal.h"

#define SCHEDULE_FILE "/data/schedule.json"
static const char *TAG = "schedule";
static esp_timer_handle_t s_save_timer;

bool schedule_load(void)
{
    size_t len = 0;
    char *text = store_read_all(SCHEDULE_FILE, &len);
    if (!text) return false;
    cJSON *root = cJSON_ParseWithLength(text, len);
    free(text);
    if (!root) {
        ESP_LOGE(TAG, "%s hong, bo qua", SCHEDULE_FILE);
        return false;
    }
    // TODO(TV3): kiểm tra "schema", đọc series, tasks, exceptions vào g_sched (trong khoá)
    cJSON_Delete(root);
    return true;
}

static void save_now(void *arg)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "schema", 1);
    ld_lock(g_sched_lock);
    cJSON_AddNumberToObject(root, "nextId", g_sched.next_id);
    // TODO(TV3): ghi series, tasks, exceptions (ngày dạng "YYYY-MM-DD", giờ dạng "HH:MM")
    ld_unlock(g_sched_lock);

    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!text) return;
    if (store_write_atomic(SCHEDULE_FILE, text, strlen(text)) != ESP_OK) ESP_LOGE(TAG, "ghi %s loi", SCHEDULE_FILE);
    free(text);
}

void schedule_mark_dirty(void)
{
    if (!s_save_timer) {
        esp_timer_create_args_t args = {.callback = save_now, .name = "schedule_save"};
        esp_timer_create(&args, &s_save_timer);
    }
    esp_timer_stop(s_save_timer);
    esp_timer_start_once(s_save_timer, 1000 * 1000);
}

void schedule_init(void)
{
    schedule_core_init();
    if (!schedule_load()) {
        ESP_LOGW(TAG, "chua co %s, dung du lieu mau", SCHEDULE_FILE);
        schedule_seed_sample();
    }
    schedule_register_cmds();
}
