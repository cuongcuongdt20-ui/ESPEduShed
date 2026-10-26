// Lưu danh mục xuống /data/catalog.json (TV2).
// Định dạng (schema 2, JSON Lines): dòng đầu {"schema":2,"nextId":..,"defaultTravel":30}, mỗi dòng sau là một bản ghi
// có "k": "school" | "campus" | "holiday" | "subject" | "class" | "travel". Nạp và ghi từng dòng nên RAM chỉ cần cỡ một
// bản ghi; dựng cả danh mục đầy thành một cây cJSON cần hàng trăm KB, vượt RAM của ESP32.
// Định dạng cũ (schema 1, một JSON object chứa các mảng) vẫn đọc được; lần ghi sau sẽ chuyển sang dạng dòng.
// Nạp: chỉ dùng dữ liệu khi cả file hợp lệ; file hỏng thì dùng dữ liệu mẫu và không ghi đè.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "platform.h"
#include "catalog.h"
#include "catalog_internal.h"
#include "dt.h"

#define CATALOG_FILE         "/data/catalog.json"
#define CATALOG_SCHEMA_OLD   1
#define CATALOG_SCHEMA_LINES 2
#define LINE_MAX_LEN         1024
static const char *TAG = "catalog";
static bool s_file_invalid;            // file có nhưng không nạp được: không tự ghi đè để còn cứu được

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

static bool parse_school(const cJSON *v, cat_school_t *s)
{
    int n;
    memset(s, 0, sizeof(*s));
    if (!j_int(v, "id", 1, UINT16_MAX, &n)) return false;
    s->id = (uint16_t) n;
    if (!j_str(v, "name", s->name, sizeof(s->name)) || !j_str(v, "shortName", s->short_name, sizeof(s->short_name))
        || !j_str(v, "color", s->color, sizeof(s->color)))
        return false;
    const cJSON *ps = cJSON_GetObjectItemCaseSensitive(v, "periods");
    if (!cJSON_IsArray(ps) || cJSON_GetArraySize(ps) > CAT_MAX_PERIODS) return false;
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

static bool parse_campus(const cJSON *v, cat_campus_t *d)
{
    int n, s;
    memset(d, 0, sizeof(*d));
    if (!j_int(v, "id", 1, UINT16_MAX, &n) || !j_int(v, "schoolId", 1, UINT16_MAX, &s)) return false;
    d->id = (uint16_t) n;
    d->school_id = (uint16_t) s;
    return j_str(v, "name", d->name, sizeof(d->name)) && j_str(v, "address", d->address, sizeof(d->address));
}

static bool parse_holiday(const cJSON *v, cat_holiday_t *d)
{
    int n, s;
    memset(d, 0, sizeof(*d));
    if (!j_int(v, "id", 1, UINT16_MAX, &n) || !j_int(v, "schoolId", 0, UINT16_MAX, &s)) return false;
    d->id = (uint16_t) n;
    d->school_id = (uint16_t) s;
    return j_str(v, "name", d->name, sizeof(d->name)) && j_date(v, "from", &d->from) && j_date(v, "to", &d->to);
}

static bool parse_subject(const cJSON *v, cat_subject_t *d)
{
    int n, c;
    memset(d, 0, sizeof(*d));
    if (!j_int(v, "id", 1, UINT16_MAX, &n) || !j_int(v, "credits", 0, 255, &c)) return false;
    d->id = (uint16_t) n;
    d->credits = (uint8_t) c;
    return j_str(v, "code", d->code, sizeof(d->code)) && j_str(v, "name", d->name, sizeof(d->name));
}

static bool parse_class(const cJSON *v, cat_class_t *d)
{
    int n, sub, sch, size;
    memset(d, 0, sizeof(*d));
    if (!j_int(v, "id", 1, UINT16_MAX, &n) || !j_int(v, "subjectId", 1, UINT16_MAX, &sub)
        || !j_int(v, "schoolId", 1, UINT16_MAX, &sch) || !j_int(v, "size", 0, UINT16_MAX, &size))
        return false;
    *d = (cat_class_t) {.id = (uint16_t) n, .subject_id = (uint16_t) sub, .school_id = (uint16_t) sch, .size = (uint16_t) size};
    return j_str(v, "code", d->code, sizeof(d->code));
}

static bool parse_travel(const cJSON *v, cat_travel_t *d)
{
    int x, y, m;
    if (!j_int(v, "campusA", 1, UINT16_MAX, &x) || !j_int(v, "campusB", 1, UINT16_MAX, &y)
        || !j_int(v, "minutes", 0, 600, &m))
        return false;
    *d = (cat_travel_t) {.campus_a = (uint16_t) x, .campus_b = (uint16_t) y, .minutes = (uint16_t) m};
    return true;
}

// Thêm một bản ghi loại kind vào c (giới hạn số lượng theo CAT_MAX_*)
static bool add_record(catalog_t *c, const char *kind, const cJSON *v)
{
#define ADD(arr, n, max, parse) ((n) < (max) && parse(v, &c->arr[n]) && ((n)++, true))
    if (!strcmp(kind, "school")) return ADD(schools, c->n_schools, CAT_MAX_SCHOOLS, parse_school);
    if (!strcmp(kind, "campus")) return ADD(campuses, c->n_campuses, CAT_MAX_CAMPUSES, parse_campus);
    if (!strcmp(kind, "holiday")) return ADD(holidays, c->n_holidays, CAT_MAX_HOLIDAYS, parse_holiday);
    if (!strcmp(kind, "subject")) return ADD(subjects, c->n_subjects, CAT_MAX_SUBJECTS, parse_subject);
    if (!strcmp(kind, "class")) return ADD(classes, c->n_classes, CAT_MAX_CLASSES, parse_class);
    if (!strcmp(kind, "travel")) return ADD(travel, c->n_travel, CAT_MAX_TRAVEL, parse_travel);
#undef ADD
    return false;
}

static bool parse_header(const cJSON *h, catalog_t *c)
{
    int n;
    if (!j_int(h, "nextId", 1, UINT16_MAX, &n)) return false;
    c->next_id = (uint16_t) n;
    if (!j_int(h, "defaultTravel", 0, 600, &n)) return false;
    c->default_travel = (uint16_t) n;
    return true;
}

// Định dạng cũ: một object chứa các mảng. Tốn RAM theo cỡ file, chỉ dùng cho file lưu bởi bản cũ.
static bool load_legacy(catalog_t *c)
{
    static const char *const keys[] = {"schools", "campuses", "holidays", "subjects", "classes", "travel"};
    static const char *const kinds[] = {"school", "campus", "holiday", "subject", "class", "travel"};
    size_t len = 0;
    char *text = store_read_all(CATALOG_FILE, &len);
    if (!text) return false;
    cJSON *root = cJSON_ParseWithLength(text, len);
    free(text);
    int schema;
    bool ok = cJSON_IsObject(root) && j_int(root, "schema", CATALOG_SCHEMA_OLD, CATALOG_SCHEMA_OLD, &schema)
              && parse_header(root, c);
    for (int k = 0; k < 6 && ok; k++) {
        const cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, keys[k]), *v;
        if (!(ok = cJSON_IsArray(arr))) break;
        cJSON_ArrayForEach(v, arr)
        {
            if (!(ok = add_record(c, kinds[k], v))) break;
        }
    }
    cJSON_Delete(root);
    return ok;
}

// Nạp thẳng vào g_cat (chỉ gọi lúc khởi động, chưa có ai dùng): không cần bản nháp catalog_t.
bool catalog_load(void)
{
    store_reader_t *r = store_open_lines(CATALOG_FILE);
    if (!r) return false;                                      // chưa có file
    char *line = malloc(LINE_MAX_LEN);
    bool ok = line != NULL, legacy = false;

    ld_lock(g_cat_lock);
    memset(&g_cat, 0, sizeof(g_cat));
    if (ok) {
        int n = store_next_line(r, line, LINE_MAX_LEN), schema;
        cJSON *head = n > 0 ? cJSON_Parse(line) : NULL;
        if (n != 0 && !(cJSON_IsObject(head) && j_int(head, "schema", CATALOG_SCHEMA_LINES, CATALOG_SCHEMA_LINES, &schema)))
            legacy = true;                                     // dòng đầu không phải header mới: file cũ (hoặc hỏng)
        else
            ok = n > 0 && parse_header(head, &g_cat);
        cJSON_Delete(head);
        while (ok && !legacy && (n = store_next_line(r, line, LINE_MAX_LEN)) != 0) {
            cJSON *v = n > 0 ? cJSON_Parse(line) : NULL;
            const cJSON *k = cJSON_GetObjectItemCaseSensitive(v, "k");
            ok = cJSON_IsString(k) && add_record(&g_cat, k->valuestring, v);
            cJSON_Delete(v);
        }
    }
    store_close_lines(r);
    free(line);
    if (ok && legacy) ok = load_legacy(&g_cat);

    if (ok) {
        ESP_LOGI(TAG, "nap %d truong, %d co so, %d lop", g_cat.n_schools, g_cat.n_campuses, g_cat.n_classes);
    } else {
        // File có nhưng không nạp được (hỏng, hoặc hết RAM để nạp): không ghi đè, để không mất dữ liệu thật
        memset(&g_cat, 0, sizeof(g_cat));
        g_cat.default_travel = 30;
        g_cat.next_id = 1;
        s_file_invalid = true;
        ESP_LOGE(TAG, "%s khong hop le hoac khong du RAM de nap; dung du lieu mau va khong ghi de", CATALOG_FILE);
    }
    ld_unlock(g_cat_lock);
    return ok;
}

// ---------- Ghi ----------

static cJSON *school_json(const cat_school_t *s)
{
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
    return o;
}

static cJSON *campus_json(const cat_campus_t *c)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "id", c->id);
    cJSON_AddNumberToObject(o, "schoolId", c->school_id);
    cJSON_AddStringToObject(o, "name", c->name);
    cJSON_AddStringToObject(o, "address", c->address);
    return o;
}

static cJSON *holiday_json(const cat_holiday_t *h)
{
    char x[11], y[11];
    dt_format_date(h->from, x);
    dt_format_date(h->to, y);
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "id", h->id);
    cJSON_AddNumberToObject(o, "schoolId", h->school_id);
    cJSON_AddStringToObject(o, "name", h->name);
    cJSON_AddStringToObject(o, "from", x);
    cJSON_AddStringToObject(o, "to", y);
    return o;
}

static cJSON *subject_json(const cat_subject_t *s)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "id", s->id);
    cJSON_AddStringToObject(o, "code", s->code);
    cJSON_AddStringToObject(o, "name", s->name);
    cJSON_AddNumberToObject(o, "credits", s->credits);
    return o;
}

static cJSON *class_json(const cat_class_t *c)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "id", c->id);
    cJSON_AddStringToObject(o, "code", c->code);
    cJSON_AddNumberToObject(o, "subjectId", c->subject_id);
    cJSON_AddNumberToObject(o, "schoolId", c->school_id);
    cJSON_AddNumberToObject(o, "size", c->size);
    return o;
}

static cJSON *travel_json(const cat_travel_t *t)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "campusA", t->campus_a);
    cJSON_AddNumberToObject(o, "campusB", t->campus_b);
    cJSON_AddNumberToObject(o, "minutes", t->minutes);
    return o;
}

// Ghi một bản ghi thành một dòng, thêm "k"; giải phóng o
static bool put_record(store_writer_t *w, cJSON *o, const char *kind)
{
    bool ok = o && cJSON_AddStringToObject(o, "k", kind);
    char *text = ok ? cJSON_PrintUnformatted(o) : NULL;
    ok = text && store_put_line(w, text);
    cJSON_free(text);
    cJSON_Delete(o);
    return ok;
}

static void save_now(void)
{
    if (s_file_invalid) {
        ESP_LOGE(TAG, "%s dang hong, khong ghi de (xoa file hoac khoi phuc ban sao luu)", CATALOG_FILE);
        return;
    }
    store_writer_t *w = store_begin(CATALOG_FILE);
    if (!w) {
        ESP_LOGE(TAG, "khong mo duoc %s de ghi", CATALOG_FILE);
        return;
    }
    // Giữ khoá suốt lần ghi để file là một ảnh nhất quán
    ld_lock(g_cat_lock);
    char head[80];
    snprintf(head, sizeof(head), "{\"schema\":%d,\"nextId\":%u,\"defaultTravel\":%u}", CATALOG_SCHEMA_LINES,
             g_cat.next_id, g_cat.default_travel);
    bool ok = store_put_line(w, head);
    for (int i = 0; i < g_cat.n_schools && ok; i++) ok = put_record(w, school_json(&g_cat.schools[i]), "school");
    for (int i = 0; i < g_cat.n_campuses && ok; i++) ok = put_record(w, campus_json(&g_cat.campuses[i]), "campus");
    for (int i = 0; i < g_cat.n_holidays && ok; i++) ok = put_record(w, holiday_json(&g_cat.holidays[i]), "holiday");
    for (int i = 0; i < g_cat.n_subjects && ok; i++) ok = put_record(w, subject_json(&g_cat.subjects[i]), "subject");
    for (int i = 0; i < g_cat.n_classes && ok; i++) ok = put_record(w, class_json(&g_cat.classes[i]), "class");
    for (int i = 0; i < g_cat.n_travel && ok; i++) ok = put_record(w, travel_json(&g_cat.travel[i]), "travel");
    ld_unlock(g_cat_lock);
    if (store_end(w, ok) != ESP_OK) ESP_LOGE(TAG, "ghi %s loi, giu ban cu", CATALOG_FILE);
}

void catalog_mark_dirty(void)
{
    // Gom nhiều thay đổi liên tiếp thành một lần ghi, sau 1 giây kể từ thay đổi cuối, trên task ghi của platform
    store_save_later(save_now);
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
