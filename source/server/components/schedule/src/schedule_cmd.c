// Lệnh TCP của lịch dạy, công việc, ngoại lệ (TV3). Đặc tả: docs/protocol.md mục 4.
//
// Thêm/sửa: kiểm tra dữ liệu → sinh buổi của bản ghi mới → conflict_check(ctx->user_id, ...) → trùng và không có
// "force":true thì trả LD_ERR_CONFLICT kèm "conflicts" → lưu → schedule_mark_dirty() → ld_push_changed_user().
// Luôn dùng ctx->user_id làm chủ bản ghi, không tin owner do client gửi. Bản ghi của người khác trả NOT_FOUND.
// Không giữ g_sched_lock khi gọi conflict_check / catalog_* (các hàm đó tự khoá).
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "cJSON.h"
#include "platform.h"
#include "schedule.h"
#include "schedule_internal.h"
#include "conflict.h"
#include "catalog.h"
#include "dt.h"

#define MAX_SERIES_DAYS 366    // một lịch lặp kéo dài tối đa một năm (tối đa 53 buổi)
#define MAX_CANDIDATES  60
#define MAX_CONFLICTS   8

// ---------- Tiện ích chung ----------

static bool take_str(const char *s, char *dst, size_t cap, bool required)
{
    if (!s) {
        if (required) return false;
        dst[0] = 0;
        return true;
    }
    size_t n = strlen(s);
    if ((required && n == 0) || n >= cap) return false;
    memcpy(dst, s, n + 1);
    return true;
}

static bool get_range(const cJSON *data, const char *key, int lo, int hi, int *out)
{
    return ld_get_int(data, key, out) && *out >= lo && *out <= hi;
}

static bool get_force(const cJSON *data)
{
    bool force = false;
    ld_get_bool(data, "force", &force);
    return force;
}

static bool series_occurs_on(const sched_series_t *s, int32_t day)
{
    if (day < s->from || day > s->to) return false;
    if (!s->weekly) return day == s->from;
    return dt_weekday(day) == s->weekday;
}

// Giờ thực của một lịch: theo tiết thì quy đổi qua bảng tiết của trường (TV2)
static bool series_slot(const sched_series_t *s, uint16_t *start, uint16_t *end)
{
    if (s->time_mode == TIME_BY_PERIOD)
        return catalog_period_to_time(s->school_id, s->first_period, s->period_count, start, end);
    *start = s->start_min;
    *end = s->end_min;
    return *start < *end && *end <= 1440;
}

// Danh sách xung đột trả về client: [{date, start, end, reason, with:{sourceId, kind, title, start, end}}].
// Bên "with" là buổi đang có (không phải ứng viên mang mã cand_id).
static void add_conflicts(cJSON *out, const conflict_t *cf, int k, uint32_t cand_id)
{
    cJSON *arr = cJSON_AddArrayToObject(out, "conflicts");
    for (int i = 0; i < k; i++) {
        const occurrence_t *other = cf[i].a.source_id == cand_id ? &cf[i].b : &cf[i].a;
        char d[11], s[6], e[6], os[6], oe[6], title[65] = "", room[17] = "", cls[17] = "";
        dt_format_date(other->day, d);
        dt_format_hm(cf[i].overlap_start, s);
        dt_format_hm(cf[i].overlap_end, e);
        dt_format_hm(other->start_min, os);
        dt_format_hm(other->end_min, oe);
        sched_label(other->source_id, other->kind, title, room, cls);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "date", d);
        cJSON_AddStringToObject(o, "start", s);
        cJSON_AddStringToObject(o, "end", e);
        cJSON_AddNumberToObject(o, "reason", cf[i].reason);
        cJSON *w = cJSON_AddObjectToObject(o, "with");
        cJSON_AddNumberToObject(w, "sourceId", other->source_id);
        cJSON_AddNumberToObject(w, "kind", other->kind);
        cJSON_AddStringToObject(w, "title", title);
        cJSON_AddStringToObject(w, "start", os);
        cJSON_AddStringToObject(w, "end", oe);
        cJSON_AddItemToArray(arr, o);
    }
}

// Kiểm tra trùng các buổi ứng viên. Trả LD_OK, hoặc LD_ERR_CONFLICT đã điền out (khi không có force).
static int check_or_conflict(const ld_ctx_t *ctx, const occurrence_t *cand, int n, uint32_t ignore, bool force,
                             cJSON *out)
{
    if (force || n == 0) return LD_OK;
    conflict_t *cf = malloc(sizeof(conflict_t) * MAX_CONFLICTS);
    if (!cf) return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    int k = conflict_check(ctx->user_id, cand, n, ignore, cf, MAX_CONFLICTS);
    if (k > 0) add_conflicts(out, cf, k, cand[0].source_id);
    free(cf);
    if (k > 0) return ld_fail(out, LD_ERR_CONFLICT, "Trung lich hoac khong du thoi gian di chuyen; gui lai voi force de van luu", NULL);
    return LD_OK;
}

// ---------- occurrence.range ----------

// occurrence.range {from, to} → {items:[{sourceId, kind, status, date, origDate, start, end, school, campus}]}
static int occurrence_range(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int32_t from, to;
    if (!dt_parse_date(ld_get_str(data, "from"), &from)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai from", "from");
    if (!dt_parse_date(ld_get_str(data, "to"), &to)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai to", "to");

    const int max = 200;
    occurrence_t *occ = malloc(sizeof(*occ) * max);
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

// ---------- Lịch dạy định kỳ ----------

static cJSON *series_json(const sched_series_t *s)
{
    char df[11], dto[11], st[6], en[6];
    dt_format_date(s->from, df);
    dt_format_date(s->to, dto);
    dt_format_hm(s->start_min, st);
    dt_format_hm(s->end_min, en);
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "id", s->id);
    cJSON_AddStringToObject(o, "subject_code", s->subject_code);
    cJSON_AddStringToObject(o, "subject_name", s->subject_name);
    cJSON_AddStringToObject(o, "class_code", s->class_code);
    cJSON_AddNumberToObject(o, "class_id", s->class_id);
    cJSON_AddNumberToObject(o, "credits", s->credits);
    cJSON_AddNumberToObject(o, "school_id", s->school_id);
    cJSON_AddNumberToObject(o, "campus_id", s->campus_id);
    cJSON_AddStringToObject(o, "room", s->room);
    cJSON_AddNumberToObject(o, "size", s->size);
    cJSON_AddNumberToObject(o, "day_of_week", s->weekday);
    cJSON_AddNumberToObject(o, "time_mode", s->time_mode);
    cJSON_AddNumberToObject(o, "period_start", s->first_period);
    cJSON_AddNumberToObject(o, "period_count", s->period_count);
    cJSON_AddStringToObject(o, "start", st);
    cJSON_AddStringToObject(o, "end", en);
    cJSON_AddStringToObject(o, "date_from", df);
    cJSON_AddStringToObject(o, "date_to", dto);
    cJSON_AddBoolToObject(o, "weekly", s->weekly);
    cJSON_AddStringToObject(o, "note", s->note);
    return o;
}

// schedule.list → {schedules:[...]} (chỉ lịch của người đăng nhập)
static int schedule_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    cJSON *arr = cJSON_AddArrayToObject(out, "schedules");
    ld_lock(g_sched_lock);
    for (int i = 0; i < g_sched.n_series; i++)
        if (g_sched.series[i].owner_id == ctx->user_id) cJSON_AddItemToArray(arr, series_json(&g_sched.series[i]));
    ld_unlock(g_sched_lock);
    return LD_OK;
}

static int find_series(uint32_t id, uint16_t owner)
{
    for (int i = 0; i < g_sched.n_series; i++)
        if (g_sched.series[i].id == id && g_sched.series[i].owner_id == owner) return i;
    return -1;
}

// schedule.get {id} → một lịch
static int schedule_get(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int id;
    if (!get_range(data, "id", 1, INT_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    ld_lock(g_sched_lock);
    int i = find_series((uint32_t) id, ctx->user_id);
    cJSON *o = i >= 0 ? series_json(&g_sched.series[i]) : NULL;
    ld_unlock(g_sched_lock);
    if (!o) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong tim thay lich day", "id");
    cJSON_AddItemToObject(out, "schedule", o);
    return LD_OK;
}

// Đọc dữ liệu một lịch dạy từ client. Trả LD_OK hoặc mã lỗi (đã điền out).
static int read_series(const cJSON *data, cJSON *out, sched_series_t *s)
{
    int v;
    if (!take_str(ld_get_str(data, "subject_code"), s->subject_code, sizeof(s->subject_code), true))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ma hoc phan 1..16 ky tu", "subject_code");
    if (!take_str(ld_get_str(data, "subject_name"), s->subject_name, sizeof(s->subject_name), true))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ten hoc phan 1..64 ky tu", "subject_name");
    if (!take_str(ld_get_str(data, "class_code"), s->class_code, sizeof(s->class_code), false))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ma lop toi da 16 ky tu", "class_code");
    if (!take_str(ld_get_str(data, "room"), s->room, sizeof(s->room), false))
        return ld_fail(out, LD_ERR_BAD_ARG, "Phong toi da 16 ky tu", "room");
    if (!take_str(ld_get_str(data, "note"), s->note, sizeof(s->note), false))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ghi chu toi da 120 ky tu", "note");
    if (ld_get_int(data, "class_id", &v) && v >= 0 && v <= UINT16_MAX) s->class_id = (uint16_t) v;
    if (ld_get_int(data, "credits", &v) && v >= 0 && v <= 20) s->credits = (uint8_t) v;
    if (ld_get_int(data, "size", &v) && v >= 0 && v <= 1000) s->size = (uint16_t) v;
    if (ld_get_int(data, "campus_id", &v) && v >= 0 && v <= UINT16_MAX) s->campus_id = (uint16_t) v;

    if (!get_range(data, "school_id", 1, UINT16_MAX, &v) || !catalog_school_exists((uint16_t) v))
        return ld_fail(out, LD_ERR_BAD_ARG, "Chon truong co trong danh muc", "school_id");
    s->school_id = (uint16_t) v;

    if (!dt_parse_date(ld_get_str(data, "date_from"), &s->from))
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai date_from", "date_from");
    s->weekly = true;
    ld_get_bool(data, "weekly", &s->weekly);
    if (s->weekly) {
        if (!dt_parse_date(ld_get_str(data, "date_to"), &s->to))
            return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai date_to", "date_to");
        if (s->to < s->from || s->to - s->from + 1 > MAX_SERIES_DAYS)
            return ld_fail(out, LD_ERR_BAD_ARG, "date_to phai sau date_from, toi da 1 nam", "date_to");
        if (!get_range(data, "day_of_week", 1, 7, &v)) return ld_fail(out, LD_ERR_BAD_ARG, "day_of_week 1..7 (1 = thu Hai)", "day_of_week");
        s->weekday = (uint8_t) v;
    } else {
        s->to = s->from;                              // lịch một lần: chỉ ngày date_from
        s->weekday = (uint8_t) dt_weekday(s->from);
    }

    int mode = TIME_BY_PERIOD;
    ld_get_int(data, "time_mode", &mode);
    if (mode == TIME_BY_PERIOD) {
        int first, count;
        if (!get_range(data, "period_start", 1, 255, &first) || !get_range(data, "period_count", 1, 20, &count))
            return ld_fail(out, LD_ERR_BAD_ARG, "Thieu period_start, period_count", "period_start");
        s->time_mode = TIME_BY_PERIOD;
        s->first_period = (uint8_t) first;
        s->period_count = (uint8_t) count;
    } else if (mode == TIME_DIRECT) {
        if (!dt_parse_hm(ld_get_str(data, "start"), &s->start_min) || !dt_parse_hm(ld_get_str(data, "end"), &s->end_min)
            || s->start_min >= s->end_min)
            return ld_fail(out, LD_ERR_BAD_ARG, "start, end dang HH:MM va end sau start", "start");
        s->time_mode = TIME_DIRECT;
    } else {
        return ld_fail(out, LD_ERR_BAD_ARG, "time_mode 0 (theo tiet) hoac 1 (theo gio)", "time_mode");
    }
    uint16_t a, b;
    if (!series_slot(s, &a, &b))
        return ld_fail(out, LD_ERR_BAD_ARG, "Khoang tiet khong co hoac vat qua cho trong trong bang tiet cua truong", "period_start");
    return LD_OK;
}

// Các buổi của lịch s (mã cand_id) để kiểm tra trùng; bỏ buổi rơi vào ngày nghỉ. Trả số buổi, -1 nếu quá nhiều.
static int series_candidates(const sched_series_t *s, uint32_t cand_id, occurrence_t *cand, int max)
{
    uint16_t a, b;
    if (!series_slot(s, &a, &b)) return 0;
    int n = 0;
    for (int32_t d = s->from; d <= s->to; d++) {
        if (!series_occurs_on(s, d)) continue;
        if (catalog_is_holiday(d, s->school_id, NULL)) continue;
        if (n >= max) return -1;
        cand[n++] = (occurrence_t) {.source_id = cand_id, .kind = OCC_TEACH, .status = OCC_NORMAL, .day = d,
                                    .orig_day = d, .start_min = a, .end_min = b, .school_id = s->school_id,
                                    .campus_id = s->campus_id};
    }
    return n;
}

// schedule.create {subject_code, subject_name, school_id, date_from, date_to, day_of_week, time_mode, period_start,
//                  period_count | start, end, ...; force?} → {id}
// schedule.update thêm id. Đổi quy tắc thì xoá ngoại lệ không còn rơi vào buổi nào, trả removedExceptions.
static int schedule_save(const ld_ctx_t *ctx, const cJSON *data, cJSON *out, bool update)
{
    int id = 0;
    if (update && !get_range(data, "id", 1, INT_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    if (update) {
        ld_lock(g_sched_lock);
        bool mine = find_series((uint32_t) id, ctx->user_id) >= 0;
        ld_unlock(g_sched_lock);
        if (!mine) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong tim thay lich day", "id");
    }

    sched_series_t s;
    memset(&s, 0, sizeof(s));
    int r = read_series(data, out, &s);
    if (r != LD_OK) return r;

    occurrence_t *cand = malloc(sizeof(occurrence_t) * MAX_CANDIDATES);
    if (!cand) return ld_fail(out, LD_ERR_INTERNAL, "Het bo nho", NULL);
    int n = series_candidates(&s, (uint32_t) id, cand, MAX_CANDIDATES);
    r = n < 0 ? ld_fail(out, LD_ERR_BAD_ARG, "Lich co qua nhieu buoi", "date_to")
              : check_or_conflict(ctx, cand, n, (uint32_t) id, get_force(data), out);
    free(cand);
    if (r != LD_OK) return r;

    int removed = 0, code = LD_OK;
    ld_lock(g_sched_lock);
    if (update) {
        int i = find_series((uint32_t) id, ctx->user_id);
        if (i < 0) {
            code = LD_ERR_NOT_FOUND;
        } else {
            s.id = (uint32_t) id;
            s.owner_id = ctx->user_id;
            g_sched.series[i] = s;
            for (int k = g_sched.n_exceptions - 1; k >= 0; k--) {
                sched_exception_t *e = &g_sched.exceptions[k];
                if (e->series_id != s.id || series_occurs_on(&s, e->orig_day)) continue;
                memmove(e, e + 1, sizeof(*e) * (size_t) (g_sched.n_exceptions - k - 1));
                g_sched.n_exceptions--;
                removed++;
            }
        }
    } else if (g_sched.n_series >= SCHED_MAX_SERIES || g_sched.next_id == 0 || g_sched.next_id == UINT32_MAX) {
        code = LD_ERR_FULL;
    } else {
        s.id = g_sched.next_id++;
        s.owner_id = ctx->user_id;
        g_sched.series[g_sched.n_series++] = s;
    }
    ld_unlock(g_sched_lock);
    if (code == LD_ERR_NOT_FOUND) return ld_fail(out, code, "Khong tim thay lich day", "id");
    if (code == LD_ERR_FULL) return ld_fail(out, code, "Da du so lich day", NULL);

    schedule_mark_dirty();
    ld_push_changed_user(ctx->user_id, "schedule", s.id);
    cJSON_AddNumberToObject(out, "id", s.id);
    if (update) cJSON_AddNumberToObject(out, "removedExceptions", removed);
    return LD_OK;
}

static int schedule_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return schedule_save(ctx, data, out, false); }
static int schedule_update(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return schedule_save(ctx, data, out, true); }

// schedule.delete {id}: xoá lịch và mọi ngoại lệ của nó
static int schedule_delete(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int id;
    if (!get_range(data, "id", 1, INT_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    ld_lock(g_sched_lock);
    int i = find_series((uint32_t) id, ctx->user_id);
    if (i >= 0) {
        memmove(&g_sched.series[i], &g_sched.series[i + 1], sizeof(sched_series_t) * (size_t) (g_sched.n_series - i - 1));
        g_sched.n_series--;
        for (int k = g_sched.n_exceptions - 1; k >= 0; k--) {
            if (g_sched.exceptions[k].series_id != (uint32_t) id) continue;
            memmove(&g_sched.exceptions[k], &g_sched.exceptions[k + 1],
                    sizeof(sched_exception_t) * (size_t) (g_sched.n_exceptions - k - 1));
            g_sched.n_exceptions--;
        }
    }
    ld_unlock(g_sched_lock);
    if (i < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong tim thay lich day", "id");
    schedule_mark_dirty();
    ld_push_changed_user(ctx->user_id, "schedule", (uint32_t) id);
    return LD_OK;
}

// ---------- Ngoại lệ một buổi ----------

// exception.set {series_id, orig_date, type, new_date?, new_start?, new_end?, new_room?, note?, force?} → {message}
// type: 1 huỷ, 2 đổi phòng, 3 đổi giờ, 4 dời buổi, 5 dạy bù (buổi gốc thành đã huỷ, thêm buổi bù ở new_date).
// Mỗi buổi gốc giữ một ngoại lệ; đặt lại thì thay ngoại lệ cũ.
static int exception_set(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int series_id, type;
    const char *new_room = ld_get_str(data, "new_room");
    const char *note = ld_get_str(data, "note");
    int32_t orig_day, new_day = 0;
    uint16_t new_start = 0, new_end = 0;

    if (!get_range(data, "series_id", 1, INT_MAX, &series_id))
        return ld_fail(out, LD_ERR_BAD_ARG, "series_id khong hop le", "series_id");
    if (!get_range(data, "type", EXC_CANCEL, EXC_MAKEUP, &type))
        return ld_fail(out, LD_ERR_BAD_ARG, "type 1..5", "type");
    if (!dt_parse_date(ld_get_str(data, "orig_date"), &orig_day))
        return ld_fail(out, LD_ERR_BAD_ARG, "orig_date phai co dang YYYY-MM-DD", "orig_date");
    if (new_room && strlen(new_room) >= sizeof(((sched_exception_t *) 0)->new_room))
        return ld_fail(out, LD_ERR_BAD_ARG, "new_room toi da 16 ky tu", "new_room");
    if (note && strlen(note) >= sizeof(((sched_exception_t *) 0)->note))
        return ld_fail(out, LD_ERR_BAD_ARG, "note toi da 64 ky tu", "note");

    bool need_time = type == EXC_TIME || type == EXC_MOVE || type == EXC_MAKEUP;
    if (type == EXC_ROOM && (!new_room || !new_room[0]))
        return ld_fail(out, LD_ERR_BAD_ARG, "Doi phong can new_room", "new_room");
    if ((type == EXC_MOVE || type == EXC_MAKEUP) && !dt_parse_date(ld_get_str(data, "new_date"), &new_day))
        return ld_fail(out, LD_ERR_BAD_ARG, "Can new_date hop le", "new_date");
    if (need_time && (!dt_parse_hm(ld_get_str(data, "new_start"), &new_start)
                      || !dt_parse_hm(ld_get_str(data, "new_end"), &new_end) || new_start >= new_end))
        return ld_fail(out, LD_ERR_BAD_ARG, "Can new_start, new_end hop le", "new_start");

    sched_exception_t candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.series_id = (uint32_t) series_id;
    candidate.orig_day = orig_day;
    candidate.type = (uint8_t) type;
    candidate.new_day = new_day;
    candidate.new_start = new_start;
    candidate.new_end = new_end;
    if (new_room) strcpy(candidate.new_room, new_room);
    if (note) strcpy(candidate.note, note);

    // Lịch phải là của người này và ngày gốc phải là ngày có buổi
    ld_lock(g_sched_lock);
    int si = find_series(candidate.series_id, ctx->user_id);
    sched_series_t series;
    if (si >= 0) series = g_sched.series[si];
    ld_unlock(g_sched_lock);
    if (si < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong tim thay lich day", "series_id");
    if (!series_occurs_on(&series, orig_day))
        return ld_fail(out, LD_ERR_BAD_ARG, "orig_date khong phai ngay co buoi hoc", "orig_date");

    // Buổi ở giờ / ngày mới không được trùng lịch khác (trừ khi force)
    if (need_time) {
        occurrence_t cand = {.source_id = candidate.series_id, .kind = type == EXC_MAKEUP ? OCC_MAKEUP : OCC_TEACH,
                             .status = OCC_CHANGED, .day = type == EXC_TIME ? orig_day : new_day, .orig_day = orig_day,
                             .start_min = new_start, .end_min = new_end, .school_id = series.school_id,
                             .campus_id = series.campus_id};
        int r = check_or_conflict(ctx, &cand, 1, candidate.series_id, get_force(data), out);
        if (r != LD_OK) return r;
    }

    ld_lock(g_sched_lock);
    int existing = -1;
    for (int i = 0; i < g_sched.n_exceptions; ++i)
        if (g_sched.exceptions[i].series_id == candidate.series_id && g_sched.exceptions[i].orig_day == orig_day) {
            existing = i;
            break;
        }
    bool full = existing < 0 && g_sched.n_exceptions >= SCHED_MAX_EXCEPTIONS;
    if (!full) {
        if (existing >= 0) g_sched.exceptions[existing] = candidate;
        else g_sched.exceptions[g_sched.n_exceptions++] = candidate;
    }
    ld_unlock(g_sched_lock);
    if (full) return ld_fail(out, LD_ERR_FULL, "Danh sach ngoai le da day", NULL);

    schedule_mark_dirty();
    ld_push_changed_user(ctx->user_id, "exception", candidate.series_id);
    cJSON_AddStringToObject(out, "message", existing >= 0 ? "Cap nhat ngoai le thanh cong" : "Tao ngoai le thanh cong");
    return LD_OK;
}

// exception.delete {series_id, orig_date}: bỏ ngoại lệ, buổi trở lại như lịch gốc
static int exception_delete(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int series_id;
    int32_t orig_day;
    if (!get_range(data, "series_id", 1, INT_MAX, &series_id))
        return ld_fail(out, LD_ERR_BAD_ARG, "series_id khong hop le", "series_id");
    if (!dt_parse_date(ld_get_str(data, "orig_date"), &orig_day))
        return ld_fail(out, LD_ERR_BAD_ARG, "orig_date phai co dang YYYY-MM-DD", "orig_date");
    ld_lock(g_sched_lock);
    int found = -1;
    if (find_series((uint32_t) series_id, ctx->user_id) >= 0) {
        for (int i = 0; i < g_sched.n_exceptions && found < 0; i++)
            if (g_sched.exceptions[i].series_id == (uint32_t) series_id && g_sched.exceptions[i].orig_day == orig_day)
                found = i;
        if (found >= 0) {
            memmove(&g_sched.exceptions[found], &g_sched.exceptions[found + 1],
                    sizeof(sched_exception_t) * (size_t) (g_sched.n_exceptions - found - 1));
            g_sched.n_exceptions--;
        }
    }
    ld_unlock(g_sched_lock);
    if (found < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co ngoai le nay", "orig_date");
    schedule_mark_dirty();
    ld_push_changed_user(ctx->user_id, "exception", (uint32_t) series_id);
    return LD_OK;
}

// ---------- Công việc ----------

// task.list → {tasks:[{id, type, title, content, day, until, start, end, deadline, campus_id, place, status, note}]}
static int task_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    cJSON *arr = cJSON_AddArrayToObject(out, "tasks");
    ld_lock(g_sched_lock);
    for (int i = 0; i < g_sched.n_tasks; i++) {
        const sched_task_t *t = &g_sched.tasks[i];
        if (t->owner_id != ctx->user_id) continue;
        char day[11], until[11], st[6], en[6];
        dt_format_date(t->day, day);
        dt_format_hm(t->start_min, st);
        dt_format_hm(t->end_min, en);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", t->id);
        cJSON_AddNumberToObject(o, "type", t->type);
        cJSON_AddStringToObject(o, "title", t->title);
        cJSON_AddStringToObject(o, "content", t->content);
        cJSON_AddStringToObject(o, "day", day);
        if (t->until > 0) {
            dt_format_date(t->until, until);
            cJSON_AddStringToObject(o, "until", until);
        } else {
            cJSON_AddNullToObject(o, "until");
        }
        cJSON_AddStringToObject(o, "start", st);
        cJSON_AddStringToObject(o, "end", en);
        cJSON_AddBoolToObject(o, "deadline", t->start_min == t->end_min);
        cJSON_AddNumberToObject(o, "campus_id", t->campus_id);
        cJSON_AddStringToObject(o, "place", t->place);
        cJSON_AddNumberToObject(o, "status", t->status);
        cJSON_AddStringToObject(o, "note", t->note);
        cJSON_AddItemToArray(arr, o);
    }
    ld_unlock(g_sched_lock);
    return LD_OK;
}

// Đọc một công việc. deadline: true = hạn chót tại start (end = start), không có khoảng giờ, không tính trùng.
static int read_task(const cJSON *data, cJSON *out, sched_task_t *t)
{
    int v;
    if (!take_str(ld_get_str(data, "title"), t->title, sizeof(t->title), true))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ten cong viec 1..64 ky tu", "title");
    if (!take_str(ld_get_str(data, "content"), t->content, sizeof(t->content), false))
        return ld_fail(out, LD_ERR_BAD_ARG, "Noi dung toi da 120 ky tu", "content");
    if (!take_str(ld_get_str(data, "place"), t->place, sizeof(t->place), false))
        return ld_fail(out, LD_ERR_BAD_ARG, "Dia diem toi da 64 ky tu", "place");
    if (!take_str(ld_get_str(data, "note"), t->note, sizeof(t->note), false))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ghi chu toi da 120 ky tu", "note");
    if (ld_get_int(data, "type", &v) && v >= 0 && v <= UINT8_MAX) t->type = (uint8_t) v;
    if (ld_get_int(data, "status", &v)) {
        if (v < 0 || v > 3) return ld_fail(out, LD_ERR_BAD_ARG, "status 0..3", "status");
        t->status = (uint8_t) v;
    }
    if (ld_get_int(data, "campus_id", &v) && v >= 0 && v <= UINT16_MAX) t->campus_id = (uint16_t) v;

    if (!dt_parse_date(ld_get_str(data, "day"), &t->day)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai day", "day");
    const char *until = ld_get_str(data, "until");
    t->until = 0;
    if (until && until[0]) {
        if (!dt_parse_date(until, &t->until) || t->until < t->day)
            return ld_fail(out, LD_ERR_BAD_ARG, "until phai tu day tro di", "until");
    }
    bool deadline = false;
    ld_get_bool(data, "deadline", &deadline);
    if (!dt_parse_hm(ld_get_str(data, "start"), &t->start_min)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai start", "start");
    if (deadline) {
        t->end_min = t->start_min;
    } else if (!dt_parse_hm(ld_get_str(data, "end"), &t->end_min) || t->end_min <= t->start_min) {
        return ld_fail(out, LD_ERR_BAD_ARG, "end phai sau start (hoac dat deadline: true)", "end");
    }
    return LD_OK;
}

static int find_task(uint32_t id, uint16_t owner)
{
    for (int i = 0; i < g_sched.n_tasks; i++)
        if (g_sched.tasks[i].id == id && g_sched.tasks[i].owner_id == owner) return i;
    return -1;
}

// task.create {title, day, start, end | deadline, until?, type?, content?, place?, campus_id?, status?, note?, force?} → {id}
// task.update thêm id
static int task_save(const ld_ctx_t *ctx, const cJSON *data, cJSON *out, bool update)
{
    int id = 0;
    if (update && !get_range(data, "id", 1, INT_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    sched_task_t t;
    memset(&t, 0, sizeof(t));
    int r = read_task(data, out, &t);
    if (r != LD_OK) return r;

    // Công việc có khoảng giờ thì kiểm tra trùng ở lần đầu tiên (lặp hàng tuần: các tuần sau cùng giờ)
    if (t.start_min < t.end_min) {
        occurrence_t cand = {.source_id = (uint32_t) id, .kind = OCC_TASK, .status = OCC_NORMAL, .day = t.day,
                             .orig_day = t.day, .start_min = t.start_min, .end_min = t.end_min, .campus_id = t.campus_id};
        r = check_or_conflict(ctx, &cand, 1, (uint32_t) id, get_force(data), out);
        if (r != LD_OK) return r;
    }

    int code = LD_OK;
    ld_lock(g_sched_lock);
    if (update) {
        int i = find_task((uint32_t) id, ctx->user_id);
        if (i < 0) code = LD_ERR_NOT_FOUND;
        else {
            t.id = (uint32_t) id;
            t.owner_id = ctx->user_id;
            g_sched.tasks[i] = t;
        }
    } else if (g_sched.n_tasks >= SCHED_MAX_TASKS || g_sched.next_id == 0 || g_sched.next_id == UINT32_MAX) {
        code = LD_ERR_FULL;
    } else {
        t.id = g_sched.next_id++;
        t.owner_id = ctx->user_id;
        g_sched.tasks[g_sched.n_tasks++] = t;
    }
    ld_unlock(g_sched_lock);
    if (code == LD_ERR_NOT_FOUND) return ld_fail(out, code, "Khong tim thay cong viec", "id");
    if (code == LD_ERR_FULL) return ld_fail(out, code, "Danh sach cong viec da day", NULL);

    schedule_mark_dirty();
    ld_push_changed_user(ctx->user_id, "task", t.id);
    cJSON_AddNumberToObject(out, "id", t.id);
    return LD_OK;
}

static int task_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return task_save(ctx, data, out, false); }
static int task_update(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return task_save(ctx, data, out, true); }

static int task_delete(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int id;
    if (!get_range(data, "id", 1, INT_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    ld_lock(g_sched_lock);
    int i = find_task((uint32_t) id, ctx->user_id);
    if (i >= 0) {
        memmove(&g_sched.tasks[i], &g_sched.tasks[i + 1], sizeof(sched_task_t) * (size_t) (g_sched.n_tasks - i - 1));
        g_sched.n_tasks--;
    }
    ld_unlock(g_sched_lock);
    if (i < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong tim thay cong viec", "id");
    schedule_mark_dirty();
    ld_push_changed_user(ctx->user_id, "task", (uint32_t) id);
    return LD_OK;
}

void schedule_register_cmds(void)
{
    ld_register_cmd("occurrence.range", occurrence_range, LD_ROLE_STUDENT);
    ld_register_cmd("schedule.list", schedule_list, LD_ROLE_STUDENT);
    ld_register_cmd("schedule.get", schedule_get, LD_ROLE_STUDENT);
    ld_register_cmd("schedule.create", schedule_create, LD_ROLE_TEACHER);   // sinh viên không tạo lịch dạy
    ld_register_cmd("schedule.update", schedule_update, LD_ROLE_TEACHER);
    ld_register_cmd("schedule.delete", schedule_delete, LD_ROLE_TEACHER);
    ld_register_cmd("exception.set", exception_set, LD_ROLE_TEACHER);
    ld_register_cmd("exception.delete", exception_delete, LD_ROLE_TEACHER);
    ld_register_cmd("task.list", task_list, LD_ROLE_STUDENT);
    ld_register_cmd("task.create", task_create, LD_ROLE_STUDENT);
    ld_register_cmd("task.update", task_update, LD_ROLE_STUDENT);
    ld_register_cmd("task.delete", task_delete, LD_ROLE_STUDENT);
}
