// Lệnh TCP lịch tuần, khoảng ngày, xung đột (TV4). BẢN GIẢ do TV1 tạo khi chuyển sang giao thức TCP:
// calendar.week và calendar.range đã chạy (chưa gắn xung đột), các lệnh conflict.* trả lỗi "chưa làm".
// TV4 viết phần thật theo README mục B4. Phản hồi phải nằm trong 8 KB của một khung TCP.
#include <stdlib.h>
#include "cJSON.h"
#include "platform.h"
#include "conflict.h"
#include "calendar_internal.h"
#include "catalog.h"
#include "dt.h"

#define MAX_OCC 120          // khoảng 60 byte JSON mỗi buổi: 120 buổi vẫn dưới 8 KB

static int not_yet(cJSON *out)
{
    return ld_fail(out, LD_ERR_INTERNAL, "Chua lam (TV4)", NULL);
}

static cJSON *occ_json(const occurrence_t *o)
{
    char d[11], od[11], s[6], e[6], title[65] = "", room[17] = "", cls[17] = "", sn[17] = "", color[8] = "#888888";
    dt_format_date(o->day, d);
    dt_format_date(o->orig_day, od);
    dt_format_hm(o->start_min, s);
    dt_format_hm(o->end_min, e);
    sched_label(o->source_id, o->kind, title, room, cls);
    catalog_school_label(o->school_id, sn, color);

    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "sourceId", o->source_id);
    cJSON_AddNumberToObject(j, "kind", o->kind);
    cJSON_AddNumberToObject(j, "status", o->status);
    cJSON_AddStringToObject(j, "date", d);
    cJSON_AddStringToObject(j, "origDate", od);
    cJSON_AddStringToObject(j, "start", s);
    cJSON_AddStringToObject(j, "end", e);
    cJSON_AddStringToObject(j, "title", title);
    cJSON_AddStringToObject(j, "room", room);
    cJSON_AddStringToObject(j, "classCode", cls);
    cJSON_AddStringToObject(j, "school", sn);
    cJSON_AddStringToObject(j, "color", color);
    // TODO(TV4): "conflicts": mã các xung đột của buổi này
    return j;
}

static int reply_range(const ld_ctx_t *ctx, int32_t from, int32_t to, cJSON *out)
{
    occurrence_t *occ = malloc(sizeof(occurrence_t) * MAX_OCC);
    if (!occ) return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    int n = sched_expand(ctx->user_id, from, to, occ, MAX_OCC);
    if (n < 0) {
        free(occ);
        return ld_fail(out, LD_ERR_BAD_ARG, "Khoang ngay qua dai hoac qua nhieu buoi", NULL);
    }
    char a[11], b[11];
    dt_format_date(from, a);
    dt_format_date(to, b);
    cJSON_AddStringToObject(out, "from", a);
    cJSON_AddStringToObject(out, "to", b);
    cJSON *items = cJSON_AddArrayToObject(out, "items");
    for (int i = 0; i < n; i++) cJSON_AddItemToArray(items, occ_json(&occ[i]));
    cJSON_AddArrayToObject(out, "conflicts");                  // TODO(TV4): conflict_find_in
    free(occ);
    return LD_OK;
}

// calendar.week {date?} → {from, to, items, conflicts}. Không có date thì dùng hôm nay của ESP.
static int calendar_week(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int32_t day;
    const char *q = ld_get_str(data, "date");
    if (q) {
        if (!dt_parse_date(q, &day)) return ld_fail(out, LD_ERR_BAD_ARG, "Sai ngay", "date");
    } else if ((day = time_today()) < 0) {
        return ld_fail(out, LD_ERR_BAD_ARG, "ESP chua co gio, hay gui date", "date");
    }
    int32_t start = dt_week_start(day);
    return reply_range(ctx, start, start + 6, out);
}

// calendar.range {from, to}
static int calendar_range(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int32_t from, to;
    if (!dt_parse_date(ld_get_str(data, "from"), &from)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai from", "from");
    if (!dt_parse_date(ld_get_str(data, "to"), &to)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai to", "to");
    return reply_range(ctx, from, to, out);
}

// TODO(TV4): conflict.list, conflict.check, conflict.ack
static int conflict_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }
static int conflict_check_cmd(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }
static int conflict_ack(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }

void calendar_register_cmds(void)
{
    ld_register_cmd("calendar.week", calendar_week, LD_ROLE_STUDENT);
    ld_register_cmd("calendar.range", calendar_range, LD_ROLE_STUDENT);
    ld_register_cmd("conflict.list", conflict_list, LD_ROLE_STUDENT);
    ld_register_cmd("conflict.check", conflict_check_cmd, LD_ROLE_STUDENT);
    ld_register_cmd("conflict.ack", conflict_ack, LD_ROLE_STUDENT);
}
