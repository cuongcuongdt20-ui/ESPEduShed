// Lệnh TCP của lịch dạy, công việc, ngoại lệ (TV3). BẢN GIẢ do TV1 tạo khi chuyển sang giao thức TCP:
// occurrence.range đã chạy, các lệnh khác trả lỗi "chưa làm". TV3 viết phần thật theo README mục B3.
//
// Thêm/sửa: kiểm tra dữ liệu → sinh buổi của bản ghi mới → conflict_check(ctx->user_id, ...) → trùng và không có
// "force":true thì trả LD_ERR_CONFLICT kèm "conflicts" → lưu → schedule_mark_dirty() → ld_push_changed_user().
// Luôn dùng ctx->user_id làm chủ bản ghi, không tin owner do client gửi.
#include <stdlib.h>
#include "cJSON.h"
#include "platform.h"
#include "schedule.h"
#include "schedule_internal.h"
#include "conflict.h"
#include "dt.h"
#include <string.h>
#include <limits.h>

static int not_yet(cJSON *out)
{
    return ld_fail(out, LD_ERR_INTERNAL, "Chua lam (TV3)", NULL);
}

// occurrence.range {from, to} → {items:[{sourceId, kind, status, date, origDate, start, end, school, campus}]}
static int occurrence_range(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int32_t from, to;
    if (!dt_parse_date(ld_get_str(data, "from"), &from)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai from", "from");
    if (!dt_parse_date(ld_get_str(data, "to"), &to)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai to", "to");

    const int max = 200;
    occurrence_t *occ = malloc(sizeof(occurrence_t) * max);
    if (!occ) return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    int n = sched_expand(ctx->user_id, from, to, occ, max);
    if (n < 0) {
        free(occ);
        return ld_fail(out, LD_ERR_BAD_ARG, "Khoang ngay qua dai hoac qua nhieu buoi", NULL);
    }
    cJSON *arr = cJSON_AddArrayToObject(out, "items");
    for (int i = 0; i < n; i++) {
        char d[11], od[11], s[6], e[6];
        dt_format_date(occ[i].day, d);
        dt_format_date(occ[i].orig_day, od);
        dt_format_hm(occ[i].start_min, s);
        dt_format_hm(occ[i].end_min, e);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "sourceId", occ[i].source_id);
        cJSON_AddNumberToObject(o, "kind", occ[i].kind);
        cJSON_AddNumberToObject(o, "status", occ[i].status);
        cJSON_AddStringToObject(o, "date", d);
        cJSON_AddStringToObject(o, "origDate", od);
        cJSON_AddStringToObject(o, "start", s);
        cJSON_AddStringToObject(o, "end", e);
        cJSON_AddNumberToObject(o, "school", occ[i].school_id);
        cJSON_AddNumberToObject(o, "campus", occ[i].campus_id);
        cJSON_AddItemToArray(arr, o);
    }
    free(occ);
    return LD_OK;
}

// TODO(TV3): schedule.list/get/create/update/delete, exception.set/delete, task.list/create/update/delete

static int schedule_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    (void)data;

    cJSON *arr = cJSON_AddArrayToObject(out, "schedules");
    if (!arr) {
        return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    }

    ld_lock(g_sched_lock);

    for (int i = 0; i < g_sched.n_series; i++) {
        const sched_series_t *s = &g_sched.series[i];

        /* Không để người dùng xem lịch của tài khoản khác. */
        if (s->owner_id != ctx->user_id) {
            continue;
        }

        char date_from[11];
        char date_to[11];

        dt_format_date(s->from, date_from);
        dt_format_date(s->to, date_to);

        cJSON *item = cJSON_CreateObject();
        if (!item) {
            ld_unlock(g_sched_lock);
            return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
        }

        cJSON_AddNumberToObject(item, "id", s->id);
        cJSON_AddStringToObject(item, "subject_code", s->subject_code);
        cJSON_AddStringToObject(item, "subject_name", s->subject_name);
        cJSON_AddStringToObject(item, "class_code", s->class_code);
        cJSON_AddNumberToObject(item, "school_id", s->school_id);
        cJSON_AddNumberToObject(item, "campus_id", s->campus_id);
        cJSON_AddNumberToObject(item, "day_of_week", s->weekday);
        cJSON_AddNumberToObject(item, "period_start", s->first_period);
        cJSON_AddNumberToObject(item, "period_count", s->period_count);
        cJSON_AddStringToObject(item, "room", s->room);
        cJSON_AddStringToObject(item, "date_from", date_from);
        cJSON_AddStringToObject(item, "date_to", date_to);

        cJSON_AddItemToArray(arr, item);
    }

    ld_unlock(g_sched_lock);
    return LD_OK;
}

static int schedule_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }
static int exception_set(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }

static int task_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    (void)data;

    cJSON *arr = cJSON_AddArrayToObject(out, "tasks");
    if (!arr) {
        return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    }

    ld_lock(g_sched_lock);

    for (int i = 0; i < g_sched.n_tasks; i++) {
        const sched_task_t *t = &g_sched.tasks[i];

        /* Chi tra cong viec cua tai khoan dang dang nhap. */
        if (t->owner_id != ctx->user_id) {
            continue;
        }

        cJSON *item = cJSON_CreateObject();
        if (!item) {
            ld_unlock(g_sched_lock);
            return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
        }

        cJSON_AddNumberToObject(item, "id", t->id);
        cJSON_AddNumberToObject(item, "type", t->type);
        cJSON_AddStringToObject(item, "title", t->title);

        cJSON_AddStringToObject(item, "content", t->content);

        char day_buf[11];
        char until_buf[11];
        char start_buf[6];
        char end_buf[6];

        dt_format_date(t->day, day_buf);
        cJSON_AddStringToObject(item, "day", day_buf);

        if (t->until > 0) {
            dt_format_date(t->until, until_buf);
            cJSON_AddStringToObject(item, "until", until_buf);
        } else {
            cJSON_AddNullToObject(item, "until");
        }

        dt_format_hm(t->start_min, start_buf);
        dt_format_hm(t->end_min, end_buf);

        cJSON_AddStringToObject(item, "start", start_buf);
        cJSON_AddStringToObject(item, "end", end_buf);

        cJSON_AddNumberToObject(item, "campus_id", t->campus_id);
        cJSON_AddStringToObject(item, "place", t->place);
        cJSON_AddNumberToObject(item, "status", t->status);
        cJSON_AddStringToObject(item, "note", t->note);

        cJSON_AddItemToArray(arr, item);
    }

    ld_unlock(g_sched_lock);
    return LD_OK;
}


static int task_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int type, status, campus_id;
    const char *title = ld_get_str(data, "title");
    const char *content = ld_get_str(data, "content");
    const char *day_str = ld_get_str(data, "day");
    const char *start_str = ld_get_str(data, "start");
    const char *end_str = ld_get_str(data, "end");
    const char *place = ld_get_str(data, "place");
    const char *note = ld_get_str(data, "note");

    int32_t day, until = 0;
    uint16_t start_min, end_min;

    if (!ctx || ctx->user_id == 0) {
        return ld_fail(out, LD_ERR_UNAUTHORIZED, "Chua dang nhap", NULL);
    }

    if (!ld_get_int(data, "type", &type) ||
        type < 0 || type > UINT8_MAX) {
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai type", "type");
    }

    if (!title || !title[0] || strlen(title) >= sizeof(((sched_task_t *)0)->title)) {
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai title", "title");
    }

    if (!content || strlen(content) >= sizeof(((sched_task_t *)0)->content)) {
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai content", "content");
    }

    if (!dt_parse_date(day_str, &day)) {
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai day", "day");
    }

    const cJSON *until_json = cJSON_GetObjectItemCaseSensitive(data, "until");
    if (until_json && !cJSON_IsNull(until_json)) {
        if (!cJSON_IsString(until_json) ||
            !dt_parse_date(until_json->valuestring, &until)) {
            return ld_fail(out, LD_ERR_BAD_ARG, "Sai until", "until");
        }

        if (until < day) {
            return ld_fail(out, LD_ERR_BAD_ARG, "until phai >= day", "until");
        }
    }

    if (!dt_parse_hm(start_str, &start_min)) {
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai start", "start");
    }

    if (!dt_parse_hm(end_str, &end_min) || end_min <= start_min) {
        return ld_fail(out, LD_ERR_BAD_ARG, "end phai sau start", "end");
    }

    if (!ld_get_int(data, "campus_id", &campus_id) ||
        campus_id < 0 || campus_id > UINT16_MAX) {
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai campus_id", "campus_id");
    }

    if (!place || strlen(place) >= sizeof(((sched_task_t *)0)->place)) {
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai place", "place");
    }

    if (!ld_get_int(data, "status", &status) ||
        status < 0 || status > UINT8_MAX) {
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai status", "status");
    }

    if (!note || strlen(note) >= sizeof(((sched_task_t *)0)->note)) {
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai note", "note");
    }

    if (strlen(title) >= 65 || strlen(content) >= 121 ||
        strlen(place) >= 65 || strlen(note) >= 121) {
        return ld_fail(out, LD_ERR_BAD_ARG, "Truong vuot qua do dai cho phep", NULL);
    }

    ld_lock(g_sched_lock);

    if (g_sched.n_tasks >= SCHED_MAX_TASKS) {
        ld_unlock(g_sched_lock);
        return ld_fail(out, LD_ERR_FULL, "Danh sach cong viec da day", NULL);
    }

    if (g_sched.next_id == 0 || g_sched.next_id == UINT32_MAX) {
        ld_unlock(g_sched_lock);
        return ld_fail(out, LD_ERR_FULL, "Het ID kha dung", NULL);
    }

    sched_task_t *t = &g_sched.tasks[g_sched.n_tasks];
    memset(t, 0, sizeof(*t));

    t->id = g_sched.next_id++;
    t->owner_id = ctx->user_id;
    t->type = (uint8_t)type;
    strcpy(t->title, title);
    strcpy(t->content, content);
    t->day = day;
    t->until = until;
    t->start_min = start_min;
    t->end_min = end_min;
    t->campus_id = (uint16_t)campus_id;
    strcpy(t->place, place);
    t->status = (uint8_t)status;
    strcpy(t->note, note);

    uint32_t id = t->id;
    g_sched.n_tasks++;

    ld_unlock(g_sched_lock);

    schedule_mark_dirty();
    ld_push_changed_user(ctx->user_id, "task", id);

    cJSON_AddNumberToObject(out, "id", id);
    cJSON_AddStringToObject(out, "message", "Tao cong viec thanh cong");

    return LD_OK;
}


void schedule_register_cmds(void)
{
    ld_register_cmd("occurrence.range", occurrence_range, LD_ROLE_STUDENT);
    ld_register_cmd("schedule.list", schedule_list, LD_ROLE_STUDENT);
    ld_register_cmd("schedule.create", schedule_create, LD_ROLE_TEACHER);   // sinh viên không tạo lịch dạy
    ld_register_cmd("exception.set", exception_set, LD_ROLE_TEACHER);
    ld_register_cmd("task.list", task_list, LD_ROLE_STUDENT);
    ld_register_cmd("task.create", task_create, LD_ROLE_STUDENT);
}
