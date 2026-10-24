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
static int occurrence_range(const ld_ctx_t *ctx,
                            const cJSON *data,
                            cJSON *out)
{
    if (!ctx || ctx->user_id == 0) {
        return ld_fail(out, LD_ERR_UNAUTHORIZED,
                       "Chua dang nhap", NULL);
    }

    int32_t from, to;
    if (!dt_parse_date(ld_get_str(data, "from"), &from)) {
        return ld_fail(out, LD_ERR_BAD_ARG,
                       "Thieu hoac sai from", "from");
    }
    if (!dt_parse_date(ld_get_str(data, "to"), &to)) {
        return ld_fail(out, LD_ERR_BAD_ARG,
                       "Thieu hoac sai to", "to");
    }

    const int max = 200;
    occurrence_t *occ = malloc(sizeof(*occ) * max);
    if (!occ) {
        return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    }

    int n = sched_expand(ctx->user_id, from, to, occ, max);
    if (n < 0) {
        free(occ);
        return ld_fail(out, LD_ERR_BAD_ARG,
                       "Khoang ngay qua dai hoac qua nhieu buoi",
                       NULL);
    }

    cJSON *arr = cJSON_AddArrayToObject(out, "items");
    if (!arr) {
        free(occ);
        return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    }

    for (int i = 0; i < n; i++) {
        char d[11], od[11], s[6], e[6];
        dt_format_date(occ[i].day, d);
        dt_format_date(occ[i].orig_day, od);
        dt_format_hm(occ[i].start_min, s);
        dt_format_hm(occ[i].end_min, e);

        cJSON *o = cJSON_CreateObject();
        if (!o) {
            free(occ);
            return ld_fail(out, LD_ERR_INTERNAL,
                           "Het bo nho", NULL);
        }

        bool ok =
            cJSON_AddNumberToObject(o, "sourceId",
                                    occ[i].source_id) &&
            cJSON_AddNumberToObject(o, "kind", occ[i].kind) &&
            cJSON_AddNumberToObject(o, "status", occ[i].status) &&
            cJSON_AddStringToObject(o, "date", d) &&
            cJSON_AddStringToObject(o, "origDate", od) &&
            cJSON_AddStringToObject(o, "start", s) &&
            cJSON_AddStringToObject(o, "end", e) &&
            cJSON_AddNumberToObject(o, "school",
                                    occ[i].school_id) &&
            cJSON_AddNumberToObject(o, "campus",
                                    occ[i].campus_id);

        if (!ok) {
            cJSON_Delete(o);
            free(occ);
            return ld_fail(out, LD_ERR_INTERNAL,
                           "Het bo nho", NULL);
        }

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

static int exception_set(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int series_id, type;
    const char *orig_date_str = ld_get_str(data, "orig_date");
    const char *new_date_str = ld_get_str(data, "new_date");
    const char *new_start_str = ld_get_str(data, "new_start");
    const char *new_end_str = ld_get_str(data, "new_end");
    const char *new_room = ld_get_str(data, "new_room");
    const char *note = ld_get_str(data, "note");

    int32_t orig_day, new_day = 0;
    uint16_t new_start = 0, new_end = 0;

    if (!ctx || ctx->user_id == 0) {
        return ld_fail(out, LD_ERR_UNAUTHORIZED,
                       "Chua xac thuc nguoi dung", NULL);
    }

    if (ctx->role != LD_ROLE_TEACHER) {
        return ld_fail(out, LD_ERR_FORBIDDEN,
                       "Chi giao vien duoc sua ngoai le lich day", NULL);
    }

    if (!ld_get_int(data, "series_id", &series_id) ||
        series_id <= 0) {
        return ld_fail(out, LD_ERR_BAD_ARG,
                       "series_id khong hop le", "series_id");
    }

    if (!ld_get_int(data, "type", &type) ||
        type < EXC_CANCEL || type > EXC_MAKEUP) {
        return ld_fail(out, LD_ERR_BAD_ARG,
                       "type khong hop le", "type");
    }

    if (!orig_date_str || !dt_parse_date(orig_date_str, &orig_day)) {
        return ld_fail(out, LD_ERR_BAD_ARG,
                       "orig_date phai co dang YYYY-MM-DD", "orig_date");
    }

    if (new_room && strlen(new_room) >= sizeof(((sched_exception_t *)0)->new_room)) {
        return ld_fail(out, LD_ERR_BAD_ARG,
                       "new_room vuot qua do dai cho phep", "new_room");
    }

    if (note && strlen(note) >= sizeof(((sched_exception_t *)0)->note)) {
        return ld_fail(out, LD_ERR_BAD_ARG,
                       "note vuot qua do dai cho phep", "note");
    }

    switch ((exc_type_t)type) {
    case EXC_CANCEL:
        break;

    case EXC_ROOM:
        if (!new_room || new_room[0] == '\0') {
            return ld_fail(out, LD_ERR_BAD_ARG,
                           "EXC_ROOM yeu cau new_room", "new_room");
        }
        break;

    case EXC_TIME:
        if (!new_start_str || !new_end_str ||
            !dt_parse_hm(new_start_str, &new_start) ||
            !dt_parse_hm(new_end_str, &new_end) ||
            new_start >= new_end) {
            return ld_fail(out, LD_ERR_BAD_ARG,
                           "EXC_TIME yeu cau new_start va new_end hop le",
                           "new_start");
        }
        break;

    case EXC_MOVE:
    case EXC_MAKEUP:
        if (!new_date_str || !dt_parse_date(new_date_str, &new_day)) {
            return ld_fail(out, LD_ERR_BAD_ARG,
                           "Loai ngoai le nay yeu cau new_date hop le",
                           "new_date");
        }

        if (!new_start_str || !new_end_str ||
            !dt_parse_hm(new_start_str, &new_start) ||
            !dt_parse_hm(new_end_str, &new_end) ||
            new_start >= new_end) {
            return ld_fail(out, LD_ERR_BAD_ARG,
                           "Can new_start va new_end hop le",
                           "new_start");
        }
        break;

    default:
        return ld_fail(out, LD_ERR_BAD_ARG, "type khong hop le", "type");
    }

    sched_exception_t candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.series_id = (uint32_t)series_id;
    candidate.orig_day = orig_day;
    candidate.type = (uint8_t)type;
    candidate.new_day = new_day;
    candidate.new_start = new_start;
    candidate.new_end = new_end;

    if (new_room) {
        strcpy(candidate.new_room, new_room);
    }
    if (note) {
        strcpy(candidate.note, note);
    }

    ld_lock(g_sched_lock);

    int series_index = -1;
    for (int i = 0; i < g_sched.n_series; ++i) {
        if (g_sched.series[i].id == candidate.series_id) {
            series_index = i;
            break;
        }
    }

    if (series_index < 0) {
        ld_unlock(g_sched_lock);
        return ld_fail(out, LD_ERR_NOT_FOUND,
                       "Khong tim thay lich day", "series_id");
    }

    const sched_series_t *series = &g_sched.series[series_index];

    if (series->owner_id != ctx->user_id) {
        ld_unlock(g_sched_lock);
        return ld_fail(out, LD_ERR_FORBIDDEN,
                       "Khong co quyen sua lich day nay", "series_id");
    }

    bool occurs_on_date =
    orig_day >= series->from &&
    orig_day <= series->to &&
    (series->weekly
         ? (series->weekday >= 1 &&
            series->weekday <= 7 &&
            dt_weekday(orig_day) == series->weekday)
         : orig_day == series->from);

    if (!occurs_on_date) {
        ld_unlock(g_sched_lock);
        return ld_fail(out, LD_ERR_BAD_ARG,
                    "orig_date khong phai ngay co buoi hoc",
                    "orig_date");
    }

    int existing_index = -1;
    for (int i = 0; i < g_sched.n_exceptions; ++i) {
        if (g_sched.exceptions[i].series_id == candidate.series_id &&
            g_sched.exceptions[i].orig_day == candidate.orig_day) {
            existing_index = i;
            break;
        }
    }

    if (existing_index < 0 &&
        g_sched.n_exceptions >= SCHED_MAX_EXCEPTIONS) {
        ld_unlock(g_sched_lock);
        return ld_fail(out, LD_ERR_FULL,
                       "Danh sach ngoai le da day", NULL);
    }

    if (existing_index >= 0) {
        g_sched.exceptions[existing_index] = candidate;
    } else {
        g_sched.exceptions[g_sched.n_exceptions++] = candidate;
    }

    ld_unlock(g_sched_lock);

    schedule_mark_dirty();
    ld_push_changed_user(ctx->user_id, "exception", candidate.series_id);

    if (!cJSON_AddStringToObject(
            out, "message",
            existing_index >= 0
                ? "Cap nhat ngoai le thanh cong"
                : "Tao ngoai le thanh cong")) {
        return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    }

    return LD_OK;
}


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
