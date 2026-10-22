// Lưu danh mục xuống /data/catalog.json (TV2).
// Định dạng: {"schema":1,"nextId":..,"defaultTravel":30,"schools":[...],"campuses":[...],"holidays":[...],
//             "subjects":[...],"classes":[...],"travel":[...]}
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "platform.h"
#include "catalog.h"
#include "catalog_internal.h"

#define CATALOG_FILE "/data/catalog.json"
static const char *TAG = "catalog";
static esp_timer_handle_t s_save_timer;

bool catalog_load(void)
{
    size_t len = 0;
    char *text = store_read_all(CATALOG_FILE, &len);
    if (!text) return false;
    cJSON *root = cJSON_ParseWithLength(text, len);
    free(text);
    if (!root) {
        ESP_LOGE(TAG, "%s hong, bo qua", CATALOG_FILE);
        return false;
    }
    // TODO(TV2): kiểm tra "schema", đọc từng mảng vào g_cat (trong khoá g_cat_lock)
    cJSON_Delete(root);
    return true;
}

static void save_now(void *arg)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "schema", 1);
    ld_lock(g_cat_lock);
    // TODO(TV2): ghi đủ mọi mảng của g_cat
    cJSON_AddNumberToObject(root, "nextId", g_cat.next_id);
    cJSON_AddNumberToObject(root, "defaultTravel", g_cat.default_travel);
    ld_unlock(g_cat_lock);

    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!text) return;
    if (store_write_atomic(CATALOG_FILE, text, strlen(text)) != ESP_OK) ESP_LOGE(TAG, "ghi %s loi", CATALOG_FILE);
    free(text);
}

void catalog_mark_dirty(void)
{
    // Gom nhiều thay đổi liên tiếp thành một lần ghi, sau 1 giây kể từ thay đổi cuối
    if (!s_save_timer) {
        esp_timer_create_args_t args = {.callback = save_now, .name = "catalog_save"};
        esp_timer_create(&args, &s_save_timer);
    }
    esp_timer_stop(s_save_timer);
    esp_timer_start_once(s_save_timer, 1000 * 1000);
    // TODO(TV2): cân nhắc chuyển việc ghi sang task riêng nếu file lớn (callback esp_timer nên ngắn)
}

void catalog_init(void)
{
    catalog_core_init();
    if (!catalog_load()) {
        ESP_LOGW(TAG, "chua co %s, dung du lieu mau", CATALOG_FILE);
        catalog_seed_sample();
    }
}
