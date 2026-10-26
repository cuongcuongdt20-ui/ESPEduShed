// Lệnh TCP lịch tuần, khoảng ngày, xung đột (TV4). Đặc tả: docs/protocol.md mục 5.
// Mọi dữ liệu lấy theo ctx->user_id của phiên, không tin user_id trong data. Phản hồi luôn dưới 8 KB:
// mỗi buổi khoảng 200 byte JSON nên calendar.* chỉ trả tối đa ITEM_BUDGET byte buổi, quá thì "truncated": true.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "platform.h"
#include "conflict.h"
#include "calendar_internal.h"
#include "catalog.h"
#include "dt.h"

#define MAX_OCC      120     // số buổi tối đa đọc từ sched_expand mỗi lần
#define ITEM_BUDGET  5800    // byte JSON tối đa cho mảng items của calendar.*
#define CONF_MAX     16      // số xung đột tối đa trong calendar.*
#define LIST_MAX     20      // số xung đột tối đa trong conflict.list
#define CHECK_MAX    8       // số xung đột tối đa trong conflict.check
#define CHUNK_DAYS   14      // conflict.list quét từng đoạn 14 ngày (xung đột chỉ xảy ra trong cùng một ngày)

static cJSON *occ_json(const occurrence_t *o, const int *cf_idx, int n_cf, int *est)
{
    char d[11], od[11], s[6], e[6], title[65] = "", room[17] = "", cls[17] = "", sn[17] = "", color[8] = "#888888";
    dt_format_date(o->day, d);
    dt_format_date(o->orig_day, od);
    dt_format_hm(o->start_min, s);
    dt_format_hm(o->end_min, e);
    sched_label(o->source_id, o->kind, title, room, cls);
    if (o->kind != OCC_TASK) sched_room_override(o->source_id, o->orig_day, room);   // buổi đổi phòng
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
    *est = 190 + (int) (strlen(title) + strlen(room) + strlen(cls) + strlen(sn));
    if (n_cf > 0) {                       // mã (chỉ số trong mảng conflicts) của các xung đột CHƯA xác nhận
        cJSON *arr = cJSON_AddArrayToObject(j, "conflicts");
        for (int i = 0; i < n_cf; i++) cJSON_AddItemToArray(arr, cJSON_CreateNumber(cf_idx[i]));
        *est += 8 * n_cf + 16;
    }
    return j;
}

// {a, b, date, start, end, reason, acked}: start-end là khoảng trùng (hoặc khoảng thiếu thời gian di chuyển)
static cJSON *conflict_json(const conflict_t *c)
{
    char d[11], s[6], e[6];
    dt_format_date(c->a.day, d);
    dt_format_hm(c->overlap_start, s);
    dt_format_hm(c->overlap_end, e);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "a", c->a.source_id);
    cJSON_AddNumberToObject(j, "b", c->b.source_id);
    cJSON_AddStringToObject(j, "date", d);
    cJSON_AddStringToObject(j, "start", s);
    cJSON_AddStringToObject(j, "end", e);
    cJSON_AddNumberToObject(j, "reason", c->reason);
    cJSON_AddBoolToObject(j, "acked", calendar_is_acked(c->a.source_id, c->b.source_id, c->a.day));
    return j;
}

static bool is_side(const occurrence_t *o, const conflict_t *c)
{
    return (o->source_id == c->a.source_id && o->start_min == c->a.start_min && o->day == c->a.day) ||
           (o->source_id == c->b.source_id && o->start_min == c->b.start_min && o->day == c->b.day);
}

static int reply_range(const ld_ctx_t *ctx, int32_t from, int32_t to, cJSON *out)
{
    occurrence_t *occ = malloc(sizeof(occurrence_t) * MAX_OCC);
    conflict_t *cf = malloc(sizeof(conflict_t) * (CONF_MAX + 1));      // +1: biết còn xung đột bị bỏ hay không
    if (!occ || !cf) {
        free(occ);
        free(cf);
        return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    }
    int n = sched_expand(ctx->user_id, from, to, occ, MAX_OCC);
    if (n < 0) {
        free(occ);
        free(cf);
        return ld_fail(out, LD_ERR_BAD_ARG, "Khoang ngay qua dai hoac qua nhieu buoi", NULL);
    }
    // Chọn các buổi đưa vào phản hồi theo ngân sách byte, rồi tìm xung đột trong đúng các buổi đó
    int est_total = 0, k = 0;
    cJSON *items = cJSON_CreateArray();
    bool truncated = false;
    int ncf = conflict_find_in(occ, n, cf, CONF_MAX + 1);
    if (ncf > CONF_MAX) {
        truncated = true;
        ncf = CONF_MAX;
    }
    for (; k < n; k++) {
        int idx[CONF_MAX], m = 0;
        for (int c = 0; c < ncf; c++)
            if (is_side(&occ[k], &cf[c]) && !calendar_is_acked(cf[c].a.source_id, cf[c].b.source_id, cf[c].a.day))
                idx[m++] = c;
        int est;
        cJSON *j = occ_json(&occ[k], idx, m, &est);
        if (est_total + est > ITEM_BUDGET) {
            cJSON_Delete(j);
            truncated = true;
            break;
        }
        est_total += est;
        cJSON_AddItemToArray(items, j);
    }
    char a[11], b[11];
    dt_format_date(from, a);
    dt_format_date(to, b);
    cJSON_AddStringToObject(out, "from", a);
    cJSON_AddStringToObject(out, "to", b);
    cJSON_AddItemToObject(out, "items", items);
    cJSON *carr = cJSON_AddArrayToObject(out, "conflicts");
    for (int c = 0; c < ncf; c++) cJSON_AddItemToArray(carr, conflict_json(&cf[c]));
    if (truncated) cJSON_AddBoolToObject(out, "truncated", true);
    free(occ);
    free(cf);
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

static bool parse_range(const cJSON *data, int32_t *from, int32_t *to, cJSON *out, int *err)
{
    if (!dt_parse_date(ld_get_str(data, "from"), from)) {
        *err = ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai from", "from");
        return false;
    }
    if (!dt_parse_date(ld_get_str(data, "to"), to)) {
        *err = ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai to", "to");
        return false;
    }
    if (*to < *from || *to - *from + 1 > SCHED_MAX_RANGE_DAYS) {
        *err = ld_fail(out, LD_ERR_BAD_ARG, "Khoang ngay khong hop le (toi da 62 ngay)", "to");
        return false;
    }
    return true;
}

// calendar.range {from, to}
static int calendar_range(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int32_t from, to;
    int err;
    if (!parse_range(data, &from, &to, out, &err)) return err;
    return reply_range(ctx, from, to, out);
}

static cJSON *side_json(const occurrence_t *o)
{
    char s[6], e[6], title[65] = "", room[17] = "", cls[17] = "";
    dt_format_hm(o->start_min, s);
    dt_format_hm(o->end_min, e);
    sched_label(o->source_id, o->kind, title, room, cls);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "sourceId", o->source_id);
    cJSON_AddNumberToObject(j, "kind", o->kind);
    cJSON_AddStringToObject(j, "title", title);
    cJSON_AddStringToObject(j, "start", s);
    cJSON_AddStringToObject(j, "end", e);
    return j;
}

// conflict.list {from, to} → {from, to, conflicts: [{a:{...}, b:{...}, date, start, end, reason, acked}], truncated?}
// Không gửi from, to: dùng tuần của hôm nay (ESP phải có giờ).
static int conflict_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int32_t from, to;
    if (ld_get_str(data, "from") || ld_get_str(data, "to")) {
        int err;
        if (!parse_range(data, &from, &to, out, &err)) return err;
    } else {
        int32_t t = time_today();
        if (t < 0) return ld_fail(out, LD_ERR_BAD_ARG, "ESP chua co gio, hay gui from, to", "from");
        from = dt_week_start(t);
        to = from + 6;
    }
    occurrence_t *occ = malloc(sizeof(occurrence_t) * MAX_OCC);
    conflict_t *cf = malloc(sizeof(conflict_t) * (LIST_MAX + 1));      // +1: biết còn xung đột bị bỏ hay không
    if (!occ || !cf) {
        free(occ);
        free(cf);
        return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    }
    char sa[11], sb[11];
    dt_format_date(from, sa);
    dt_format_date(to, sb);
    cJSON_AddStringToObject(out, "from", sa);
    cJSON_AddStringToObject(out, "to", sb);
    cJSON *arr = cJSON_AddArrayToObject(out, "conflicts");
    int total = 0;
    bool truncated = false;
    for (int32_t d = from; d <= to && !truncated; d += CHUNK_DAYS) {
        int32_t e = d + CHUNK_DAYS - 1 < to ? d + CHUNK_DAYS - 1 : to;
        int n = sched_expand(ctx->user_id, d, e, occ, MAX_OCC);
        if (n < 0) {
            free(occ);
            free(cf);
            return ld_fail(out, LD_ERR_INTERNAL, "Qua nhieu buoi trong mot doan 14 ngay", NULL);
        }
        int k = conflict_find_in(occ, n, cf, LIST_MAX - total + 1);
        if (total + k > LIST_MAX) {
            truncated = true;
            k = LIST_MAX - total;
        }
        for (int i = 0; i < k; i++) {
            cJSON *j = conflict_json(&cf[i]);
            cJSON_DeleteItemFromObject(j, "a");
            cJSON_DeleteItemFromObject(j, "b");
            cJSON_AddItemToObject(j, "a", side_json(&cf[i].a));
            cJSON_AddItemToObject(j, "b", side_json(&cf[i].b));
            cJSON_AddItemToArray(arr, j);
        }
        total += k;
    }
    if (truncated) cJSON_AddBoolToObject(out, "truncated", true);
    free(occ);
    free(cf);
    return LD_OK;
}

// conflict.check: kiểm tra một bản nháp, không lưu gì.
//   cách 1 (dời một buổi có sẵn): {sourceId, kind?, origDate, curDate?, date, start, end}
//          curDate = ngày buổi đang nằm (khác origDate khi buổi đã bị dời hoặc là buổi dạy bù); mặc định origDate
//   cách 2 (buổi mới):            {date, start, end, schoolId?, campusId?, ignoreSourceId?}
// → {conflicts: [{with: {sourceId, kind, title, start, end}, date, start, end, reason}]}
static int conflict_check_cmd(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int32_t day;
    uint16_t s, e;
    if (!dt_parse_date(ld_get_str(data, "date"), &day)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai date", "date");
    if (!dt_parse_hm(ld_get_str(data, "start"), &s)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai start", "start");
    if (!dt_parse_hm(ld_get_str(data, "end"), &e)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai end", "end");
    if (s >= e) return ld_fail(out, LD_ERR_BAD_ARG, "end phai sau start", "end");

    occurrence_t cand = {.kind = OCC_TEACH, .status = OCC_NORMAL, .day = day, .orig_day = day, .start_min = s, .end_min = e};
    uint32_t ignore = 0;
    int sid;
    if (ld_get_int(data, "sourceId", &sid)) {
        int32_t orig, cur;
        int kind = 0;
        if (!dt_parse_date(ld_get_str(data, "origDate"), &orig)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai origDate", "origDate");
        const char *cur_s = ld_get_str(data, "curDate");
        if (!cur_s) cur = orig;
        else if (!dt_parse_date(cur_s, &cur)) return ld_fail(out, LD_ERR_BAD_ARG, "Sai curDate", "curDate");
        ld_get_int(data, "kind", &kind);
        occurrence_t *one = malloc(sizeof(occurrence_t) * 64);
        if (!one) return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
        // Tìm buổi ở ngày nó đang nằm; buổi dạy bù và buổi gốc (đã huỷ) cùng orig_day thì phân biệt bằng kind
        int n = sched_expand(ctx->user_id, cur, cur, one, 64), found = -1;
        for (int i = 0; i < n; i++)
            if (one[i].source_id == (uint32_t) sid && one[i].orig_day == orig && (kind == 0 || one[i].kind == kind)
                && one[i].status != OCC_CANCELLED)
                found = i;
        if (found < 0) {
            free(one);
            return ld_fail(out, LD_ERR_NOT_FOUND, "Khong tim thay buoi", "sourceId");   // cũng là kết quả khi buổi của người khác
        }
        cand = one[found];
        free(one);
        cand.day = day;
        cand.start_min = s;
        cand.end_min = e;
        ignore = (uint32_t) sid;
    } else {
        int v;
        if (ld_get_int(data, "schoolId", &v) && v > 0 && v < 65536) cand.school_id = (uint16_t) v;
        if (ld_get_int(data, "campusId", &v) && v > 0 && v < 65536) cand.campus_id = (uint16_t) v;
        if (ld_get_int(data, "ignoreSourceId", &v) && v > 0) ignore = (uint32_t) v;
    }

    conflict_t *cf = malloc(sizeof(conflict_t) * CHECK_MAX);
    if (!cf) return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    int k = conflict_check(ctx->user_id, &cand, 1, ignore, cf, CHECK_MAX);
    char d[11];
    dt_format_date(day, d);
    cJSON *arr = cJSON_AddArrayToObject(out, "conflicts");
    for (int i = 0; i < k; i++) {
        char cs[6], ce[6];
        dt_format_hm(cf[i].overlap_start, cs);
        dt_format_hm(cf[i].overlap_end, ce);
        // bên "khác" là buổi không phải ứng viên (conflict_check đã bỏ buổi cùng source_id)
        const occurrence_t *other = cf[i].a.start_min == cand.start_min && cf[i].a.end_min == cand.end_min &&
                                            cf[i].a.source_id == cand.source_id && cf[i].a.day == cand.day &&
                                            cf[i].a.orig_day == cand.orig_day
                                        ? &cf[i].b
                                        : &cf[i].a;
        cJSON *j = cJSON_CreateObject();
        cJSON_AddItemToObject(j, "with", side_json(other));
        cJSON_AddStringToObject(j, "date", d);
        cJSON_AddStringToObject(j, "start", cs);
        cJSON_AddStringToObject(j, "end", ce);
        cJSON_AddNumberToObject(j, "reason", cf[i].reason);
        cJSON_AddItemToArray(arr, j);
    }
    free(cf);
    return LD_OK;
}

// conflict.ack {a, b, date}: đánh dấu "đã biết". Chỉ nhận khi cặp đó đang là xung đột thật trong lịch của người
// dùng này; ngược lại NOT_FOUND (không lộ lịch của người khác).
static int conflict_ack(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int a, b;
    int32_t day;
    if (!ld_get_int(data, "a", &a) || a <= 0) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu a", "a");
    if (!ld_get_int(data, "b", &b) || b <= 0) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu b", "b");
    if (!dt_parse_date(ld_get_str(data, "date"), &day)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai date", "date");

    occurrence_t *occ = malloc(sizeof(occurrence_t) * 64);
    conflict_t *cf = malloc(sizeof(conflict_t) * LIST_MAX);
    if (!occ || !cf) {
        free(occ);
        free(cf);
        return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    }
    int n = sched_expand(ctx->user_id, day, day, occ, 64);
    int k = n > 0 ? conflict_find_in(occ, n, cf, LIST_MAX) : 0;
    bool real = false;
    for (int i = 0; i < k && !real; i++)
        real = (cf[i].a.source_id == (uint32_t) a && cf[i].b.source_id == (uint32_t) b) ||
               (cf[i].a.source_id == (uint32_t) b && cf[i].b.source_id == (uint32_t) a);
    free(occ);
    free(cf);
    if (!real) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co xung dot nay", NULL);
    if (!calendar_ack_add((uint32_t) a, (uint32_t) b, day)) return ld_fail(out, LD_ERR_FULL, "Het cho luu xac nhan", NULL);
    ld_push_changed_user(ctx->user_id, "conflict", 0);
    return LD_OK;
}

void calendar_register_cmds(void)
{
    ld_register_cmd("calendar.week", calendar_week, LD_ROLE_STUDENT);
    ld_register_cmd("calendar.range", calendar_range, LD_ROLE_STUDENT);
    ld_register_cmd("conflict.list", conflict_list, LD_ROLE_STUDENT);
    ld_register_cmd("conflict.check", conflict_check_cmd, LD_ROLE_STUDENT);
    ld_register_cmd("conflict.ack", conflict_ack, LD_ROLE_STUDENT);
}
