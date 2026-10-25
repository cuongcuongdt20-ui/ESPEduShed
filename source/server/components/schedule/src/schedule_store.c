
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "platform.h"
#include "schedule.h"
#include "schedule_internal.h"
#include "dt.h"

#define SCHEDULE_FILE "/data/schedule.json"
#define SCHEDULE_SCHEMA 1

static const char *TAG = "schedule";
static esp_timer_handle_t s_save_timer;
/* Khong tu dong ghi de file neu file lich ton tai nhung khong hop le. */
static bool s_storage_invalid;

/* ---------- JSON đọc an toàn ---------- */



static bool json_int(const cJSON *obj, const char *key,
                     int64_t min_value, int64_t max_value,
                     int64_t *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);

    if (!cJSON_IsNumber(v)) {
        return false;
    }

    double d = v->valuedouble;

    /* Kiem tra gioi han truoc khi chuyen sang int64_t. */
    if (d != d ||
        d < (double)min_value ||
        d > (double)max_value) {
        return false;
    }

    int64_t n = (int64_t)d;

    /* Tu choi so thap phan, chi chap nhan so nguyen. */
    if ((double)n != d) {
        return false;
    }

    *out = n;
    return true;
}



static bool json_string(const cJSON *obj, const char *key,
                        char *out, size_t capacity)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);

    if (!cJSON_IsString(v) || !v->valuestring ||
        strlen(v->valuestring) >= capacity) {
        return false;
    }

    memcpy(out, v->valuestring, strlen(v->valuestring) + 1);
    return true;
}

static bool json_date(const cJSON *obj, const char *key, int32_t *out)
{
    char buf[16];

    if (!json_string(obj, key, buf, sizeof(buf))) {
        return false;
    }

    /* Yêu cầu đúng định dạng YYYY-MM-DD. */
    if (strlen(buf) != 10 || buf[4] != '-' || buf[7] != '-') {
        return false;
    }

    int32_t day;
    if (!dt_parse_date(buf, &day)) {
        return false;
    }

    *out = day;
    return true;
}

static bool json_time(const cJSON *obj, const char *key, uint16_t *out,
                      bool allow_24)
{
    char buf[8];

    if (!json_string(obj, key, buf, sizeof(buf))) {
        return false;
    }

    if (allow_24 && strcmp(buf, "24:00") == 0) {
        *out = 1440;
        return true;
    }

    if (strlen(buf) != 5 || buf[2] != ':') {
        return false;
    }

    return dt_parse_hm(buf, out);
}

static bool json_bool(const cJSON *obj, const char *key, bool *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);

    if (cJSON_IsTrue(v)) {
        *out = true;
        return true;
    }
    if (cJSON_IsFalse(v)) {
        *out = false;
        return true;
    }

    return false;
}

static bool valid_time_range(uint16_t start, uint16_t end)
{
    return start < end && end <= 1440;
}

/* ---------- Đọc từng loại bản ghi ---------- */

static bool parse_series(const cJSON *v, sched_series_t *s)
{
    int64_t n;

    memset(s, 0, sizeof(*s));

    if (!json_int(v, "id", 1, UINT32_MAX, &n)) return false;
    s->id = (uint32_t)n;

    if (!json_int(v, "ownerId", 1, UINT16_MAX, &n)) return false;
    s->owner_id = (uint16_t)n;

    if (!json_int(v, "classId", 0, UINT16_MAX, &n)) return false;
    s->class_id = (uint16_t)n;

    if (!json_string(v, "subjectCode", s->subject_code,
                     sizeof(s->subject_code))) return false;
    if (!json_string(v, "subjectName", s->subject_name,
                     sizeof(s->subject_name))) return false;
    if (!json_string(v, "classCode", s->class_code,
                     sizeof(s->class_code))) return false;
    if (!json_string(v, "room", s->room, sizeof(s->room))) return false;
    if (!json_string(v, "note", s->note, sizeof(s->note))) return false;

    if (!json_int(v, "credits", 0, UINT8_MAX, &n)) return false;
    s->credits = (uint8_t)n;

    if (!json_int(v, "schoolId", 0, UINT16_MAX, &n)) return false;
    s->school_id = (uint16_t)n;
    if (!json_int(v, "campusId", 0, UINT16_MAX, &n)) return false;
    s->campus_id = (uint16_t)n;
    if (!json_int(v, "size", 0, UINT16_MAX, &n)) return false;
    s->size = (uint16_t)n;

    if (!json_int(v, "weekday", 1, 7, &n)) return false;
    s->weekday = (uint8_t)n;
    if (!json_int(v, "timeMode", TIME_BY_PERIOD, TIME_DIRECT, &n))
        return false;
    s->time_mode = (uint8_t)n;

    if (!json_int(v, "firstPeriod", 0, UINT8_MAX, &n)) return false;
    s->first_period = (uint8_t)n;
    if (!json_int(v, "periodCount", 0, UINT8_MAX, &n)) return false;
    s->period_count = (uint8_t)n;

    if (!json_time(v, "start", &s->start_min, false)) return false;
    if (!json_time(v, "end", &s->end_min, true)) return false;

    if (!json_date(v, "from", &s->from)) return false;
    if (!json_date(v, "to", &s->to)) return false;
    if (s->to < s->from) return false;
    if (!json_bool(v, "weekly", &s->weekly)) return false;

    if (s->time_mode == TIME_DIRECT &&
        !valid_time_range(s->start_min, s->end_min)) {
        return false;
    }
    if (s->time_mode == TIME_BY_PERIOD &&
        (s->first_period == 0 || s->period_count == 0)) {
        return false;
    }

    return true;
}

static bool parse_task(const cJSON *v, sched_task_t *t)
{
    int64_t n;

    memset(t, 0, sizeof(*t));

    if (!json_int(v, "id", 1, UINT32_MAX, &n)) return false;
    t->id = (uint32_t)n;
    if (!json_int(v, "ownerId", 1, UINT16_MAX, &n)) return false;
    t->owner_id = (uint16_t)n;
    if (!json_int(v, "type", 0, UINT8_MAX, &n)) return false;
    t->type = (uint8_t)n;

    if (!json_string(v, "title", t->title, sizeof(t->title))) return false;
    if (!json_string(v, "content", t->content, sizeof(t->content)))
        return false;
    if (!json_string(v, "place", t->place, sizeof(t->place))) return false;
    if (!json_string(v, "note", t->note, sizeof(t->note))) return false;

    if (!json_date(v, "day", &t->day)) return false;

    const cJSON *until = cJSON_GetObjectItemCaseSensitive(v, "until");
    if (cJSON_IsNull(until)) {
        t->until = 0;
    } else if (!json_date(v, "until", &t->until)) {
        return false;
    }

    if (!json_time(v, "start", &t->start_min, false)) return false;
    if (!json_time(v, "end", &t->end_min, true)) return false;
    // start == end: công việc kiểu hạn chót
    if (t->start_min > t->end_min || t->end_min > 1440) return false;

    if (t->until != 0 && t->until < t->day) return false;

    if (!json_int(v, "campusId", 0, UINT16_MAX, &n)) return false;
    t->campus_id = (uint16_t)n;
    if (!json_int(v, "status", 0, UINT8_MAX, &n)) return false;
    t->status = (uint8_t)n;

    return true;
}

static bool parse_exception(const cJSON *v, sched_exception_t *e)
{
    int64_t n;

    memset(e, 0, sizeof(*e));

    if (!json_int(v, "seriesId", 1, UINT32_MAX, &n)) return false;
    e->series_id = (uint32_t)n;
    if (!json_date(v, "origDay", &e->orig_day)) return false;
    if (!json_int(v, "type", EXC_CANCEL, EXC_MAKEUP, &n)) return false;
    e->type = (uint8_t)n;

    const cJSON *new_day = cJSON_GetObjectItemCaseSensitive(v, "newDay");
    if (cJSON_IsNull(new_day)) {
        e->new_day = 0;
    } else if (!json_date(v, "newDay", &e->new_day)) {
        return false;
    }

    if (!json_time(v, "newStart", &e->new_start, false)) return false;
    if (!json_time(v, "newEnd", &e->new_end, true)) return false;
    if (!json_string(v, "newRoom", e->new_room, sizeof(e->new_room)))
        return false;
    if (!json_string(v, "note", e->note, sizeof(e->note))) return false;

    if ((e->type == EXC_TIME || e->type == EXC_MOVE ||
         e->type == EXC_MAKEUP) &&
        !valid_time_range(e->new_start, e->new_end)) {
        return false;
    }

    if ((e->type == EXC_MOVE || e->type == EXC_MAKEUP) &&
        e->new_day == 0) {
        return false;
    }

    return true;
}

/* ---------- Load: chỉ cập nhật g_sched khi toàn bộ file hợp lệ ---------- */

bool schedule_load(void)
{
    size_t len = 0;
    char *text = store_read_all(SCHEDULE_FILE, &len);

    s_storage_invalid = false;

    if (!text) {
        return false; /* Chua co file hoac khong doc duoc. */
    }

    cJSON *root = cJSON_ParseWithLength(text, len);
    free(text);

    if (!cJSON_IsObject(root)) {
        ESP_LOGE(TAG, "%s khong phai JSON object hop le", SCHEDULE_FILE);
        cJSON_Delete(root);
        s_storage_invalid = true;
        return false;
    }

    int64_t schema, next_id;
    const cJSON *series = cJSON_GetObjectItemCaseSensitive(root, "series");
    const cJSON *tasks = cJSON_GetObjectItemCaseSensitive(root, "tasks");
    const cJSON *exceptions =
        cJSON_GetObjectItemCaseSensitive(root, "exceptions");

    bool ok =
        json_int(root, "schema", SCHEDULE_SCHEMA, SCHEDULE_SCHEMA, &schema) &&
        json_int(root, "nextId", 1, UINT32_MAX, &next_id) &&
        cJSON_IsArray(series) &&
        cJSON_IsArray(tasks) &&
        cJSON_IsArray(exceptions) &&
        cJSON_GetArraySize(series) <= SCHED_MAX_SERIES &&
        cJSON_GetArraySize(tasks) <= SCHED_MAX_TASKS &&
        cJSON_GetArraySize(exceptions) <= SCHED_MAX_EXCEPTIONS;

    schedule_t *tmp = NULL;

    if (ok) {
        tmp = calloc(1, sizeof(*tmp));
        if (!tmp) ok = false;
    }

    if (ok) {
        tmp->next_id = (uint32_t)next_id;

        const cJSON *item;
        cJSON_ArrayForEach(item, series) {
            if (!parse_series(item, &tmp->series[tmp->n_series])) {
                ok = false;
                break;
            }
            tmp->n_series++;
        }
    }

    if (ok) {
        const cJSON *item;
        cJSON_ArrayForEach(item, tasks) {
            if (!parse_task(item, &tmp->tasks[tmp->n_tasks])) {
                ok = false;
                break;
            }
            tmp->n_tasks++;
        }
    }

    if (ok) {
        const cJSON *item;
        cJSON_ArrayForEach(item, exceptions) {
            if (!parse_exception(item,
                    &tmp->exceptions[tmp->n_exceptions])) {
                ok = false;
                break;
            }
            tmp->n_exceptions++;
        }
    }

    /* Kiểm tra ID duy nhất trong từng loại và tham chiếu ngoại lệ. */
    if (ok) {
        for (int i = 0; i < tmp->n_series && ok; ++i) {
            for (int j = i + 1; j < tmp->n_series; ++j) {
                if (tmp->series[i].id == tmp->series[j].id) {
                    ok = false;
                    break;
                }
            }
        }

        for (int i = 0; i < tmp->n_tasks && ok; ++i) {
            for (int j = i + 1; j < tmp->n_tasks; ++j) {
                if (tmp->tasks[i].id == tmp->tasks[j].id) {
                    ok = false;
                    break;
                }
            }
        }

        for (int i = 0; i < tmp->n_exceptions && ok; ++i) {
            bool found = false;

            for (int j = 0; j < tmp->n_series; ++j) {
                if (tmp->series[j].id == tmp->exceptions[i].series_id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                ok = false;
                break;
            }

            for (int j = i + 1; j < tmp->n_exceptions; ++j) {
                if (tmp->exceptions[i].series_id ==
                        tmp->exceptions[j].series_id &&
                    tmp->exceptions[i].orig_day ==
                        tmp->exceptions[j].orig_day) {
                    ok = false;
                    break;
                }
            }
        }
    }

    if (ok) {
        ld_lock(g_sched_lock);
        memcpy(&g_sched, tmp, sizeof(g_sched));
        ld_unlock(g_sched_lock);
    } else {
        s_storage_invalid = true;
        ESP_LOGE(TAG, "%s co schema hoac du lieu khong hop le",
                 SCHEDULE_FILE);
    }

    free(tmp);
    cJSON_Delete(root);
    return ok;
}

/* ---------- JSON ghi ---------- */

static bool add_date(cJSON *obj, const char *key, int32_t day)
{
    char buf[11];
    dt_format_date(day, buf);
    return cJSON_AddStringToObject(obj, key, buf) != NULL;
}

static bool add_time(cJSON *obj, const char *key, uint16_t min)
{
    char buf[8];

    if (min == 1440) {
        memcpy(buf, "24:00", 6);
    } else if (min < 1440) {
        dt_format_hm(min, buf);
    } else {
        return false;
    }

    return cJSON_AddStringToObject(obj, key, buf) != NULL;
}

static bool add_series_json(cJSON *a, const sched_series_t *s)
{
    cJSON *o = cJSON_CreateObject();
    if (!o) return false;
    if (!cJSON_AddItemToArray(a, o)) {
        cJSON_Delete(o);
        return false;
    }

#define ADD_NUM(k, v) do { if (!cJSON_AddNumberToObject(o, k, (double)(v))) return false; } while (0)
#define ADD_STR(k, v) do { if (!cJSON_AddStringToObject(o, k, v)) return false; } while (0)

    ADD_NUM("id", s->id);
    ADD_NUM("ownerId", s->owner_id);
    ADD_NUM("classId", s->class_id);
    ADD_STR("subjectCode", s->subject_code);
    ADD_STR("subjectName", s->subject_name);
    ADD_STR("classCode", s->class_code);
    ADD_NUM("credits", s->credits);
    ADD_NUM("schoolId", s->school_id);
    ADD_NUM("campusId", s->campus_id);
    ADD_STR("room", s->room);
    ADD_NUM("size", s->size);
    ADD_NUM("weekday", s->weekday);
    ADD_NUM("timeMode", s->time_mode);
    ADD_NUM("firstPeriod", s->first_period);
    ADD_NUM("periodCount", s->period_count);
    if (!cJSON_AddBoolToObject(o, "weekly", s->weekly)) {
        return false;
    }
    ADD_STR("note", s->note);

    if (!add_time(o, "start", s->start_min) ||
        !add_time(o, "end", s->end_min) ||
        !add_date(o, "from", s->from) ||
        !add_date(o, "to", s->to)) return false;

#undef ADD_NUM
#undef ADD_STR
    return true;
}

static bool add_task_json(cJSON *a, const sched_task_t *t)
{
    cJSON *o = cJSON_CreateObject();
    if (!o) return false;
    if (!cJSON_AddItemToArray(a, o)) {
        cJSON_Delete(o);
        return false;
    }

#define ADD_NUM(k, v) do { if (!cJSON_AddNumberToObject(o, k, (double)(v))) return false; } while (0)
#define ADD_STR(k, v) do { if (!cJSON_AddStringToObject(o, k, v)) return false; } while (0)

    ADD_NUM("id", t->id);
    ADD_NUM("ownerId", t->owner_id);
    ADD_NUM("type", t->type);
    ADD_STR("title", t->title);
    ADD_STR("content", t->content);
    ADD_NUM("status", t->status);
    ADD_NUM("campusId", t->campus_id);
    ADD_STR("place", t->place);
    ADD_STR("note", t->note);

    if (!add_date(o, "day", t->day)) return false;

    if (t->until == 0) {
        if (!cJSON_AddNullToObject(o, "until")) return false;
    } else if (!add_date(o, "until", t->until)) {
        return false;
    }

    if (!add_time(o, "start", t->start_min) ||
        !add_time(o, "end", t->end_min)) return false;

#undef ADD_NUM
#undef ADD_STR
    return true;
}

static bool add_exception_json(cJSON *a, const sched_exception_t *e)
{
    cJSON *o = cJSON_CreateObject();
    if (!o) return false;
    if (!cJSON_AddItemToArray(a, o)) {
        cJSON_Delete(o);
        return false;
    }

#define ADD_NUM(k, v) do { if (!cJSON_AddNumberToObject(o, k, (double)(v))) return false; } while (0)
#define ADD_STR(k, v) do { if (!cJSON_AddStringToObject(o, k, v)) return false; } while (0)

    ADD_NUM("seriesId", e->series_id);
    ADD_NUM("type", e->type);
    ADD_STR("newRoom", e->new_room);
    ADD_STR("note", e->note);

    if (!add_date(o, "origDay", e->orig_day)) return false;

    if (e->type == EXC_MOVE || e->type == EXC_MAKEUP) {
        if (!add_date(o, "newDay", e->new_day)) return false;
    } else if (!cJSON_AddNullToObject(o, "newDay")) {
        return false;
    }

    if (!add_time(o, "newStart", e->new_start) ||
        !add_time(o, "newEnd", e->new_end)) return false;

#undef ADD_NUM
#undef ADD_STR
    return true;
}

static void save_now(void *arg)
{
    (void)arg;
    if (s_storage_invalid) {
        ESP_LOGE(TAG,
                "bo qua ghi %s de tranh ghi de du lieu loi",
                SCHEDULE_FILE);
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *series = cJSON_CreateArray();
    cJSON *tasks = cJSON_CreateArray();
    cJSON *exceptions = cJSON_CreateArray();

    if (!root || !series || !tasks || !exceptions) {
        cJSON_Delete(root);
        cJSON_Delete(series);
        cJSON_Delete(tasks);
        cJSON_Delete(exceptions);
        return;
    }

    bool ok = true;

    ld_lock(g_sched_lock);

    ok = cJSON_AddNumberToObject(root, "schema", SCHEDULE_SCHEMA) &&
         cJSON_AddNumberToObject(root, "nextId", g_sched.next_id) &&
         cJSON_AddItemToObject(root, "series", series) &&
         cJSON_AddItemToObject(root, "tasks", tasks) &&
         cJSON_AddItemToObject(root, "exceptions", exceptions);

    for (int i = 0; i < g_sched.n_series && ok; ++i)
        ok = add_series_json(series, &g_sched.series[i]);

    for (int i = 0; i < g_sched.n_tasks && ok; ++i)
        ok = add_task_json(tasks, &g_sched.tasks[i]);

    for (int i = 0; i < g_sched.n_exceptions && ok; ++i)
        ok = add_exception_json(exceptions, &g_sched.exceptions[i]);

    ld_unlock(g_sched_lock);

    if (!ok) {
        ESP_LOGE(TAG, "tao JSON lich that bai");
        cJSON_Delete(root);
        return;
    }

    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!text) {
        ESP_LOGE(TAG, "khong du bo nho tao JSON lich");
        return;
    }

    esp_err_t err = store_write_atomic(SCHEDULE_FILE, text, strlen(text));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ghi %s loi: %s", SCHEDULE_FILE, esp_err_to_name(err));
    }

    free(text);
}

void schedule_mark_dirty(void)
{
    if (!s_save_timer) {
        esp_timer_create_args_t args = {
            .callback = save_now,
            .name = "schedule_save"
        };

        if (esp_timer_create(&args, &s_save_timer) != ESP_OK) {
            ESP_LOGE(TAG, "tao save timer that bai");
            return;
        }
    }

    esp_timer_stop(s_save_timer);

    if (esp_timer_start_once(s_save_timer, 1000 * 1000) != ESP_OK) {
        ESP_LOGE(TAG, "khoi dong save timer that bai");
    }
}

void schedule_init(void)
{
    schedule_core_init();

    if (!schedule_load()) {
        if (s_storage_invalid) {
            ESP_LOGE(TAG,
                    "%s ton tai nhung khong hop le; khong dung du lieu mau",
                    SCHEDULE_FILE);
        } else {
            s_storage_invalid = true;
            ESP_LOGW(TAG,
                    "chua co %s; dung du lieu mau",
                    SCHEDULE_FILE);
            schedule_seed_sample();
        }
    }

    schedule_register_cmds();
}
