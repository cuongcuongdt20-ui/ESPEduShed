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
static int schedule_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }
static int schedule_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }
static int exception_set(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }
static int task_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }
static int task_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }

void schedule_register_cmds(void)
{
    ld_register_cmd("occurrence.range", occurrence_range, LD_ROLE_STUDENT);
    ld_register_cmd("schedule.list", schedule_list, LD_ROLE_STUDENT);
    ld_register_cmd("schedule.create", schedule_create, LD_ROLE_TEACHER);   // sinh viên không tạo lịch dạy
    ld_register_cmd("exception.set", exception_set, LD_ROLE_TEACHER);
    ld_register_cmd("task.list", task_list, LD_ROLE_STUDENT);
    ld_register_cmd("task.create", task_create, LD_ROLE_STUDENT);
}
