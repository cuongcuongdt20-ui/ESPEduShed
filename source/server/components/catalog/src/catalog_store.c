// Lưu danh mục xuống /data/catalog.json (TV2).
// Định dạng: {"schema":1,"nextId":..,"defaultTravel":30,"schools":[...],"campuses":[...],"holidays":[...],
//             "subjects":[...],"classes":[...],"travel":[...]}
// Nạp: đọc toàn bộ vào bản tạm, chỉ thay g_cat khi cả file hợp lệ; file hỏng thì giữ dữ liệu mẫu và không ghi đè.
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "platform.h"
#include "catalog.h"
#include "catalog_internal.h"
#include "dt.h"

#define CATALOG_FILE   "/data/catalog.json"
#define CATALOG_SCHEMA 1
static const char *TAG = "catalog";
static esp_timer_handle_t s_save_timer;
static bool s_file_invalid;            // file có nhưng hỏng: không tự ghi đè để còn cứu được

// ---------- Đọc ----------

static bool j_int(const cJSON *o, const char *key, int lo, int hi, int *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsNumber(v) || v->valuedouble != (double) (int) v->valuedouble) return false;
    int n = (int) v->valuedouble;
    if (n < lo || n > hi) return false;
    *out = n;
    return true;
}

static bool j_str(const cJSON *o, const char *key, char *dst, size_t cap)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsString(v) || strlen(v->valuestring) >= cap) return false;
    strcpy(dst, v->valuestring);
    return true;
}

static bool j_hm(const cJSON *o, const char *key, uint16_t *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) && dt_parse_hm(v->valuestring, out);
}

static bool j_date(const cJSON *o, const char *key, int32_t *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) && dt_parse_date(v->valuestring, out);
}

// Lấy mảng key, kiểm tra không quá max phần tử
static const cJSON *j_array(const cJSON *root, const char *key, int max)
{
    const cJSON *a = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsArray(a) && cJSON_GetArraySize(a) <= max ? a : NULL;
}

static bool parse_school(const cJSON *v, cat_school_t *s)
{
    int n;
    if (!j_int(v, "id", 1, UINT16_MAX, &n)) return false;
    s->id = (uint16_t) n;
    if (!j_str(v, "name", s->name, sizeof(s->name)) || !j_str(v, "shortName", s->short_name, sizeof(s->short_name))
        || !j_str(v, "color", s->color, sizeof(s->color)))
        return false;
    const cJSON *ps = j_array(v, "periods", CAT_MAX_PERIODS);
    if (!ps) return false;
    const cJSON *p;
    cJSON_ArrayForEach(p, ps)
    {
        cat_period_t *d = &s->periods[s->n_periods];
        if (!j_int(p, "no", 1, 255, &n) || !j_hm(p, "start", &d->start) || !j_hm(p, "end", &d->end) || d->start >= d->end)
            return false;
        d->no = (uint8_t) n;
        s->n_periods++;
    }
    return true;
}

static bool parse_all(const cJSON *root, catalog_t *c)
{
    int n;
    if (!j_int(root, "schema", CATALOG_SCHEMA, CATALOG_SCHEMA, &n)) return false;
    if (!j_int(root, "nextId", 1, UINT16_MAX, &n)) return false;
    c->next_id = (uint16_t) n;
    if (!j_int(root, "defaultTravel", 0, 600, &n)) return false;
    c->default_travel = (uint16_t) n;

    const cJSON *a, *v;
    if (!(a = j_array(root, "schools", CAT_MAX_SCHOOLS))) return false;
    cJSON_ArrayForEach(v, a)
    {
        if (!parse_school(v, &c->schools[c->n_schools])) return false;
        c->n_schools++;
    }
    if (!(a = j_array(root, "campuses", CAT_MAX_CAMPUSES))) return false;
    cJSON_ArrayForEach(v, a)
    {
        cat_campus_t *d = &c->campuses[c->n_campuses];
        if (!j_int(v, "id", 1, UINT16_MAX, &n)) return false;
        d->id = (uint16_t) n;
        if (!j_int(v, "schoolId", 1, UINT16_MAX, &n)) return false;
        d->school_id = (uint16_t) n;
        if (!j_str(v, "name", d->name, sizeof(d->name)) || !j_str(v, "address", d->address, sizeof(d->address)))
            return false;
        c->n_campuses++;
    }
    if (!(a = j_array(root, "holidays", CAT_MAX_HOLIDAYS))) return false;
    cJSON_ArrayForEach(v, a)
    {
        cat_holiday_t *d = &c->holidays[c->n_holidays];
        if (!j_int(v, "id", 1, UINT16_MAX, &n)) return false;
        d->id = (uint16_t) n;
        if (!j_int(v, "schoolId", 0, UINT16_MAX, &n)) return false;
        d->school_id = (uint16_t) n;
        if (!j_str(v, "name", d->name, sizeof(d->name)) || !j_date(v, "from", &d->from) || !j_date(v, "to", &d->to))
            return false;
        c->n_holidays++;
    }
    if (!(a = j_array(root, "subjects", CAT_MAX_SUBJECTS))) return false;
    cJSON_ArrayForEach(v, a)
    {
        cat_subject_t *d = &c->subjects[c->n_subjects];
        if (!j_int(v, "id", 1, UINT16_MAX, &n)) return false;
        d->id = (uint16_t) n;
        if (!j_str(v, "code", d->code, sizeof(d->code)) || !j_str(v, "name", d->name, sizeof(d->name))) return false;
        if (!j_int(v, "credits", 0, 255, &n)) return false;
        d->credits = (uint8_t) n;
        c->n_subjects++;
    }
    if (!(a = j_array(root, "classes", CAT_MAX_CLASSES))) return false;
    cJSON_ArrayForEach(v, a)
    {
        cat_class_t *d = &c->classes[c->n_classes];
        int sub, sch, size;
        if (!j_int(v, "id", 1, UINT16_MAX, &n) || !j_int(v, "subjectId", 1, UINT16_MAX, &sub)
            || !j_int(v, "schoolId", 1, UINT16_MAX, &sch) || !j_int(v, "size", 0, UINT16_MAX, &size))
            return false;
        d->id = (uint16_t) n;
        d->subject_id = (uint16_t) sub;
        d->school_id = (uint16_t) sch;
        d->size = (uint16_t) size;
        if (!j_str(v, "code", d->code, sizeof(d->code))) return false;
        c->n_classes++;
    }
    if (!(a = j_array(root, "travel", CAT_MAX_TRAVEL))) return false;
    cJSON_ArrayForEach(v, a)
    {
        cat_travel_t *d = &c->travel[c->n_travel];
        int x, y, m;
        if (!j_int(v, "campusA", 1, UINT16_MAX, &x) || !j_int(v, "campusB", 1, UINT16_MAX, &y)
            || !j_int(v, "minutes", 0, 600, &m))
            return false;
        *d = (cat_travel_t) {.campus_a = (uint16_t) x, .campus_b = (uint16_t) y, .minutes = (uint16_t) m};
        c->n_travel++;
    }
    return true;
}

bool catalog_load(void)
{
    size_t len = 0;
    char *text = store_read_all(CATALOG_FILE, &len);
    if (!text) return false;
    cJSON *root = cJSON_ParseWithLength(text, len);
    free(text);
    catalog_t *tmp = calloc(1, sizeof(*tmp));
    bool ok = tmp && cJSON_IsObject(root) && parse_all(root, tmp);
    cJSON_Delete(root);
    if (ok) {
        ld_lock(g_cat_lock);
        g_cat = *tmp;
        ld_unlock(g_cat_lock);
        ESP_LOGI(TAG, "nap %d truong, %d co so, %d lop", tmp->n_schools, tmp->n_campuses, tmp->n_classes);
    } else {
        s_file_invalid = tmp != NULL;   // hết RAM thì không coi là file hỏng
        ESP_LOGE(TAG, "%s hong hoac khong doc duoc, dung du lieu mau va khong ghi de", CATALOG_FILE);
    }
    free(tmp);
    return ok;
}

// ---------- Ghi ----------

static cJSON *build_json(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "schema", CATALOG_SCHEMA);
    cJSON_AddNumberToObject(root, "nextId", g_cat.next_id);
    cJSON_AddNumberToObject(root, "defaultTravel", g_cat.default_travel);

    cJSON *a = cJSON_AddArrayToObject(root, "schools");
    for (int i = 0; i < g_cat.n_schools; i++) {
        const cat_school_t *s = &g_cat.schools[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", s->id);
        cJSON_AddStringToObject(o, "name", s->name);
        cJSON_AddStringToObject(o, "shortName", s->short_name);
        cJSON_AddStringToObject(o, "color", s->color);
        cJSON *ps = cJSON_AddArrayToObject(o, "periods");
        for (int k = 0; k < s->n_periods; k++) {
            char x[6], y[6];
            dt_format_hm(s->periods[k].start, x);
            dt_format_hm(s->periods[k].end, y);
            cJSON *p = cJSON_CreateObject();
            cJSON_AddNumberToObject(p, "no", s->periods[k].no);
            cJSON_AddStringToObject(p, "start", x);
            cJSON_AddStringToObject(p, "end", y);
            cJSON_AddItemToArray(ps, p);
        }
        cJSON_AddItemToArray(a, o);
    }
    a = cJSON_AddArrayToObject(root, "campuses");
    for (int i = 0; i < g_cat.n_campuses; i++) {
        const cat_campus_t *c = &g_cat.campuses[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", c->id);
        cJSON_AddNumberToObject(o, "schoolId", c->school_id);
        cJSON_AddStringToObject(o, "name", c->name);
        cJSON_AddStringToObject(o, "address", c->address);
        cJSON_AddItemToArray(a, o);
    }
    a = cJSON_AddArrayToObject(root, "holidays");
    for (int i = 0; i < g_cat.n_holidays; i++) {
        const cat_holiday_t *h = &g_cat.holidays[i];
        char x[11], y[11];
        dt_format_date(h->from, x);
        dt_format_date(h->to, y);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", h->id);
        cJSON_AddNumberToObject(o, "schoolId", h->school_id);
        cJSON_AddStringToObject(o, "name", h->name);
        cJSON_AddStringToObject(o, "from", x);
        cJSON_AddStringToObject(o, "to", y);
        cJSON_AddItemToArray(a, o);
    }
    a = cJSON_AddArrayToObject(root, "subjects");
    for (int i = 0; i < g_cat.n_subjects; i++) {
        const cat_subject_t *s = &g_cat.subjects[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", s->id);
        cJSON_AddStringToObject(o, "code", s->code);
        cJSON_AddStringToObject(o, "name", s->name);
        cJSON_AddNumberToObject(o, "credits", s->credits);
        cJSON_AddItemToArray(a, o);
    }
    a = cJSON_AddArrayToObject(root, "classes");
    for (int i = 0; i < g_cat.n_classes; i++) {
        const cat_class_t *c = &g_cat.classes[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", c->id);
        cJSON_AddStringToObject(o, "code", c->code);
        cJSON_AddNumberToObject(o, "subjectId", c->subject_id);
        cJSON_AddNumberToObject(o, "schoolId", c->school_id);
        cJSON_AddNumberToObject(o, "size", c->size);
        cJSON_AddItemToArray(a, o);
    }
    a = cJSON_AddArrayToObject(root, "travel");
    for (int i = 0; i < g_cat.n_travel; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "campusA", g_cat.travel[i].campus_a);
        cJSON_AddNumberToObject(o, "campusB", g_cat.travel[i].campus_b);
        cJSON_AddNumberToObject(o, "minutes", g_cat.travel[i].minutes);
        cJSON_AddItemToArray(a, o);
    }
    return root;
}

static void save_now(void *arg)
{
    if (s_file_invalid) {
        ESP_LOGE(TAG, "%s dang hong, khong ghi de (xoa file hoac khoi phuc ban sao luu)", CATALOG_FILE);
        return;
    }
    ld_lock(g_cat_lock);
    cJSON *root = build_json();
    ld_unlock(g_cat_lock);
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!text) {
        ESP_LOGE(TAG, "het bo nho khi ghi %s", CATALOG_FILE);
        return;
    }
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
}

void catalog_init(void)
{
    catalog_core_init();
    if (!catalog_load()) {
        ESP_LOGW(TAG, "chua co %s hop le, dung du lieu mau", CATALOG_FILE);
        catalog_seed_sample();
    }
    catalog_register_cmds();
}
