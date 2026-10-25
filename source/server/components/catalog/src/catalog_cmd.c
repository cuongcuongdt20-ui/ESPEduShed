// Lệnh TCP của danh mục (TV2): trường, cơ sở, bảng tiết, ngày nghỉ, môn, lớp, thời gian di chuyển.
// Đặc tả: docs/protocol.md mục 3. Đọc: student trở lên; ghi: teacher trở lên.
//
// Mỗi lệnh ghi: kiểm tra dữ liệu → sửa g_cat trong khoá → catalog_mark_dirty() → ld_push_changed().
// Không giữ g_cat_lock khi gọi sang module khác (sched_uses_*), để không có vòng chờ khoá.
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "platform.h"
#include "catalog.h"
#include "catalog_internal.h"
#include "dt.h"
#include "schedule.h"

#define MAX_TRAVEL_MINUTES 600

// ---------- Kiểm tra dữ liệu vào ----------

// Chép chuỗi s vào dst (dung lượng cap kể cả '\0'). Sai nếu thiếu, rỗng (khi required) hoặc quá dài.
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

static bool valid_color(const char *c)
{
    if (!c || strlen(c) != 7 || c[0] != '#') return false;
    for (int i = 1; i < 7; i++) {
        char ch = c[i];
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return false;
    }
    return true;
}

static bool get_range(const cJSON *data, const char *key, int lo, int hi, int *out)
{
    return ld_get_int(data, key, out) && *out >= lo && *out <= hi;
}

// Mã mới cho mọi loại bản ghi danh mục. Gọi trong khoá. 0 = hết mã.
static uint16_t new_id(void)
{
    if (g_cat.next_id == 0 || g_cat.next_id == UINT16_MAX) return 0;
    return g_cat.next_id++;
}

static void changed(const char *entity, uint32_t id)
{
    catalog_mark_dirty();
    ld_push_changed(entity, id);
}

// ---------- Tìm theo mã (gọi trong khoá) ----------

static int school_idx(uint16_t id)
{
    for (int i = 0; i < g_cat.n_schools; i++)
        if (g_cat.schools[i].id == id) return i;
    return -1;
}

static int campus_idx(uint16_t id)
{
    for (int i = 0; i < g_cat.n_campuses; i++)
        if (g_cat.campuses[i].id == id) return i;
    return -1;
}

static int holiday_idx(uint16_t id)
{
    for (int i = 0; i < g_cat.n_holidays; i++)
        if (g_cat.holidays[i].id == id) return i;
    return -1;
}

static int subject_idx(uint16_t id)
{
    for (int i = 0; i < g_cat.n_subjects; i++)
        if (g_cat.subjects[i].id == id) return i;
    return -1;
}

static int class_idx(uint16_t id)
{
    for (int i = 0; i < g_cat.n_classes; i++)
        if (g_cat.classes[i].id == id) return i;
    return -1;
}

static int travel_idx(uint16_t a, uint16_t b)
{
    for (int i = 0; i < g_cat.n_travel; i++) {
        const cat_travel_t *t = &g_cat.travel[i];
        if ((t->campus_a == a && t->campus_b == b) || (t->campus_a == b && t->campus_b == a)) return i;
    }
    return -1;
}

// Xoá phần tử i của mảng arr (n phần tử, mỗi phần tử size byte) bằng cách dồn các phần tử sau lên
#define REMOVE_AT(arr, n, i) \
    do { \
        memmove(&(arr)[i], &(arr)[(i) + 1], sizeof((arr)[0]) * (size_t) ((n) - (i) - 1)); \
        (n)--; \
    } while (0)

// Bỏ mọi thời gian di chuyển dính tới cơ sở id. Gọi trong khoá.
static void drop_travel_of(uint16_t campus_id)
{
    for (int i = g_cat.n_travel - 1; i >= 0; i--)
        if (g_cat.travel[i].campus_a == campus_id || g_cat.travel[i].campus_b == campus_id)
            REMOVE_AT(g_cat.travel, g_cat.n_travel, i);
}

// ---------- Trường ----------

static cJSON *school_json(const cat_school_t *s)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "id", s->id);
    cJSON_AddStringToObject(o, "name", s->name);
    cJSON_AddStringToObject(o, "shortName", s->short_name);
    cJSON_AddStringToObject(o, "color", s->color);
    cJSON *ps = cJSON_AddArrayToObject(o, "periods");
    for (int i = 0; i < s->n_periods; i++) {
        char a[6], b[6];
        dt_format_hm(s->periods[i].start, a);
        dt_format_hm(s->periods[i].end, b);
        cJSON *p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "no", s->periods[i].no);
        cJSON_AddStringToObject(p, "start", a);
        cJSON_AddStringToObject(p, "end", b);
        cJSON_AddItemToArray(ps, p);
    }
    return o;
}

// school.list → {schools:[{id, name, shortName, color, periods:[{no, start, end}]}]}
static int school_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    cJSON *arr = cJSON_AddArrayToObject(out, "schools");
    ld_lock(g_cat_lock);
    for (int i = 0; i < g_cat.n_schools; i++) cJSON_AddItemToArray(arr, school_json(&g_cat.schools[i]));
    ld_unlock(g_cat_lock);
    return LD_OK;
}

// Đọc {name, shortName, color} vào s. Trả LD_OK hoặc mã lỗi.
static int read_school(const cJSON *data, cJSON *out, cat_school_t *s)
{
    if (!take_str(ld_get_str(data, "name"), s->name, sizeof(s->name), true))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ten truong 1..64 ky tu", "name");
    if (!take_str(ld_get_str(data, "shortName"), s->short_name, sizeof(s->short_name), true))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ten viet tat 1..16 ky tu", "shortName");
    const char *color = ld_get_str(data, "color");
    if (!color || !color[0]) color = "#2563EB";
    if (!valid_color(color)) return ld_fail(out, LD_ERR_BAD_ARG, "Mau dang #RRGGBB", "color");
    strcpy(s->color, color);
    return LD_OK;
}

// school.create {name, shortName, color} → {id}
static int school_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    cat_school_t s = {0};
    int r = read_school(data, out, &s);
    if (r != LD_OK) return r;
    ld_lock(g_cat_lock);
    if (g_cat.n_schools >= CAT_MAX_SCHOOLS || !(s.id = new_id())) {
        ld_unlock(g_cat_lock);
        return ld_fail(out, LD_ERR_FULL, "Da du so truong", NULL);
    }
    g_cat.schools[g_cat.n_schools++] = s;
    ld_unlock(g_cat_lock);
    changed("school", s.id);
    cJSON_AddNumberToObject(out, "id", s.id);
    return LD_OK;
}

// school.update {id, name, shortName, color}: giữ nguyên bảng tiết
static int school_update(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int id;
    if (!get_range(data, "id", 1, UINT16_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    cat_school_t s = {0};
    int r = read_school(data, out, &s);
    if (r != LD_OK) return r;
    ld_lock(g_cat_lock);
    int i = school_idx((uint16_t) id);
    if (i >= 0) {
        cat_school_t *d = &g_cat.schools[i];
        strcpy(d->name, s.name);
        strcpy(d->short_name, s.short_name);
        strcpy(d->color, s.color);
    }
    ld_unlock(g_cat_lock);
    if (i < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co truong nay", "id");
    changed("school", (uint32_t) id);
    cJSON_AddNumberToObject(out, "id", id);
    return LD_OK;
}

// school.delete {id}: lịch đang dùng trường hoặc trường còn lớp → CONFLICT; xoá kèm cơ sở và ngày nghỉ riêng
static int school_delete(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int id;
    if (!get_range(data, "id", 1, UINT16_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    if (sched_uses_school((uint16_t) id))
        return ld_fail(out, LD_ERR_CONFLICT, "Truong dang duoc lich day su dung", "id");
    ld_lock(g_cat_lock);
    int i = school_idx((uint16_t) id);
    bool has_class = false;
    for (int k = 0; k < g_cat.n_classes && !has_class; k++) has_class = g_cat.classes[k].school_id == id;
    if (i >= 0 && !has_class) {
        REMOVE_AT(g_cat.schools, g_cat.n_schools, i);
        for (int k = g_cat.n_campuses - 1; k >= 0; k--) {
            if (g_cat.campuses[k].school_id != id) continue;
            drop_travel_of(g_cat.campuses[k].id);
            REMOVE_AT(g_cat.campuses, g_cat.n_campuses, k);
        }
        for (int k = g_cat.n_holidays - 1; k >= 0; k--)
            if (g_cat.holidays[k].school_id == id) REMOVE_AT(g_cat.holidays, g_cat.n_holidays, k);
    }
    ld_unlock(g_cat_lock);
    if (i < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co truong nay", "id");
    if (has_class) return ld_fail(out, LD_ERR_CONFLICT, "Truong con lop hoc phan, xoa lop truoc", "id");
    changed("school", (uint32_t) id);
    return LD_OK;
}

// periods.set {schoolId, periods:[{no, start, end}]}: thay cả bảng tiết; số tiết không trùng, giờ không chồng nhau
static int periods_set(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int sid;
    if (!get_range(data, "schoolId", 1, UINT16_MAX, &sid)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu schoolId", "schoolId");
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(data, "periods");
    if (!cJSON_IsArray(arr)) return ld_fail(out, LD_ERR_BAD_ARG, "periods phai la mang", "periods");
    int n = cJSON_GetArraySize(arr);
    if (n > CAT_MAX_PERIODS) return ld_fail(out, LD_ERR_FULL, "Toi da 20 tiet moi truong", "periods");

    cat_period_t ps[CAT_MAX_PERIODS];
    int k = 0;
    const cJSON *it;
    cJSON_ArrayForEach(it, arr)
    {
        int no;
        cat_period_t p;
        if (!get_range(it, "no", 1, 255, &no) || !dt_parse_hm(ld_get_str(it, "start"), &p.start)
            || !dt_parse_hm(ld_get_str(it, "end"), &p.end) || p.start >= p.end)
            return ld_fail(out, LD_ERR_BAD_ARG, "Moi tiet can no 1..255, start < end dang HH:MM", "periods");
        p.no = (uint8_t) no;
        // chèn giữ thứ tự tăng theo giờ bắt đầu
        int pos = k;
        while (pos > 0 && ps[pos - 1].start > p.start) {
            ps[pos] = ps[pos - 1];
            pos--;
        }
        ps[pos] = p;
        k++;
    }
    for (int i = 0; i < k; i++) {
        for (int j = i + 1; j < k; j++)
            if (ps[i].no == ps[j].no) return ld_fail(out, LD_ERR_BAD_ARG, "So tiet bi trung", "periods");
        if (i > 0 && ps[i].start < ps[i - 1].end)
            return ld_fail(out, LD_ERR_BAD_ARG, "Hai tiet chong gio nhau", "periods");
    }

    ld_lock(g_cat_lock);
    int i = school_idx((uint16_t) sid);
    if (i >= 0) {
        memcpy(g_cat.schools[i].periods, ps, sizeof(ps[0]) * (size_t) k);
        g_cat.schools[i].n_periods = (uint8_t) k;
    }
    ld_unlock(g_cat_lock);
    if (i < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co truong nay", "schoolId");
    changed("school", (uint32_t) sid);
    return LD_OK;
}

// ---------- Cơ sở ----------

// campus.list {schoolId?} → {campuses:[{id, schoolId, name, address}]}
static int campus_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int sid = 0;
    ld_get_int(data, "schoolId", &sid);
    cJSON *arr = cJSON_AddArrayToObject(out, "campuses");
    ld_lock(g_cat_lock);
    for (int i = 0; i < g_cat.n_campuses; i++) {
        const cat_campus_t *c = &g_cat.campuses[i];
        if (sid > 0 && c->school_id != sid) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", c->id);
        cJSON_AddNumberToObject(o, "schoolId", c->school_id);
        cJSON_AddStringToObject(o, "name", c->name);
        cJSON_AddStringToObject(o, "address", c->address);
        cJSON_AddItemToArray(arr, o);
    }
    ld_unlock(g_cat_lock);
    return LD_OK;
}

static int read_campus(const cJSON *data, cJSON *out, cat_campus_t *c)
{
    int sid;
    if (!get_range(data, "schoolId", 1, UINT16_MAX, &sid)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu schoolId", "schoolId");
    c->school_id = (uint16_t) sid;
    if (!take_str(ld_get_str(data, "name"), c->name, sizeof(c->name), true))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ten co so 1..64 ky tu", "name");
    if (!take_str(ld_get_str(data, "address"), c->address, sizeof(c->address), false))
        return ld_fail(out, LD_ERR_BAD_ARG, "Dia chi toi da 128 ky tu", "address");
    return LD_OK;
}

// campus.create {schoolId, name, address?} → {id}; campus.update thêm id
static int campus_save(const ld_ctx_t *ctx, const cJSON *data, cJSON *out, bool update)
{
    int id = 0;
    if (update && !get_range(data, "id", 1, UINT16_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    cat_campus_t c = {0};
    int r = read_campus(data, out, &c);
    if (r != LD_OK) return r;
    ld_lock(g_cat_lock);
    if (school_idx(c.school_id) < 0) {
        ld_unlock(g_cat_lock);
        return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co truong nay", "schoolId");
    }
    int code = LD_OK;
    if (update) {
        int i = campus_idx((uint16_t) id);
        if (i < 0) code = LD_ERR_NOT_FOUND;
        else {
            c.id = (uint16_t) id;
            g_cat.campuses[i] = c;
        }
    } else if (g_cat.n_campuses >= CAT_MAX_CAMPUSES || !(c.id = new_id())) {
        code = LD_ERR_FULL;
    } else {
        g_cat.campuses[g_cat.n_campuses++] = c;
    }
    ld_unlock(g_cat_lock);
    if (code == LD_ERR_NOT_FOUND) return ld_fail(out, code, "Khong co co so nay", "id");
    if (code == LD_ERR_FULL) return ld_fail(out, code, "Da du so co so", NULL);
    changed("campus", c.id);
    cJSON_AddNumberToObject(out, "id", c.id);
    return LD_OK;
}

static int campus_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return campus_save(ctx, data, out, false); }
static int campus_update(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return campus_save(ctx, data, out, true); }

// campus.delete {id}: xoá kèm thời gian di chuyển của cơ sở đó
static int campus_delete(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int id;
    if (!get_range(data, "id", 1, UINT16_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    ld_lock(g_cat_lock);
    int i = campus_idx((uint16_t) id);
    if (i >= 0) {
        drop_travel_of((uint16_t) id);
        REMOVE_AT(g_cat.campuses, g_cat.n_campuses, i);
    }
    ld_unlock(g_cat_lock);
    if (i < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co co so nay", "id");
    changed("campus", (uint32_t) id);
    return LD_OK;
}

// ---------- Ngày nghỉ ----------

// holiday.list {schoolId?} → {holidays:[{id, schoolId, name, fromDate, toDate}]}; schoolId 0 = mọi trường
static int holiday_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int sid = 0;
    ld_get_int(data, "schoolId", &sid);
    cJSON *arr = cJSON_AddArrayToObject(out, "holidays");
    ld_lock(g_cat_lock);
    for (int i = 0; i < g_cat.n_holidays; i++) {
        const cat_holiday_t *h = &g_cat.holidays[i];
        if (sid > 0 && h->school_id != 0 && h->school_id != sid) continue;
        char a[11], b[11];
        dt_format_date(h->from, a);
        dt_format_date(h->to, b);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", h->id);
        cJSON_AddNumberToObject(o, "schoolId", h->school_id);
        cJSON_AddStringToObject(o, "name", h->name);
        cJSON_AddStringToObject(o, "fromDate", a);
        cJSON_AddStringToObject(o, "toDate", b);
        cJSON_AddItemToArray(arr, o);
    }
    ld_unlock(g_cat_lock);
    return LD_OK;
}

// holiday.create {schoolId, name, from, to} → {id}; holiday.update thêm id
static int holiday_save(const ld_ctx_t *ctx, const cJSON *data, cJSON *out, bool update)
{
    int id = 0, sid = 0;
    if (update && !get_range(data, "id", 1, UINT16_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    cat_holiday_t h = {0};
    if (ld_get_int(data, "schoolId", &sid) && (sid < 0 || sid > UINT16_MAX))
        return ld_fail(out, LD_ERR_BAD_ARG, "schoolId sai", "schoolId");
    h.school_id = (uint16_t) sid;
    if (!take_str(ld_get_str(data, "name"), h.name, sizeof(h.name), true))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ten ngay nghi 1..64 ky tu", "name");
    if (!dt_parse_date(ld_get_str(data, "from"), &h.from)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai from", "from");
    if (!dt_parse_date(ld_get_str(data, "to"), &h.to)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu hoac sai to", "to");
    if (h.to < h.from) return ld_fail(out, LD_ERR_BAD_ARG, "to phai tu from tro di", "to");

    ld_lock(g_cat_lock);
    int code = LD_OK;
    if (h.school_id != 0 && school_idx(h.school_id) < 0) {
        code = LD_ERR_BAD_ARG;
    } else if (update) {
        int i = holiday_idx((uint16_t) id);
        if (i < 0) code = LD_ERR_NOT_FOUND;
        else {
            h.id = (uint16_t) id;
            g_cat.holidays[i] = h;
        }
    } else if (g_cat.n_holidays >= CAT_MAX_HOLIDAYS || !(h.id = new_id())) {
        code = LD_ERR_FULL;
    } else {
        g_cat.holidays[g_cat.n_holidays++] = h;
    }
    ld_unlock(g_cat_lock);
    if (code == LD_ERR_BAD_ARG) return ld_fail(out, code, "Khong co truong nay", "schoolId");
    if (code == LD_ERR_NOT_FOUND) return ld_fail(out, code, "Khong co ngay nghi nay", "id");
    if (code == LD_ERR_FULL) return ld_fail(out, code, "Da du so ngay nghi", NULL);
    changed("holiday", h.id);
    cJSON_AddNumberToObject(out, "id", h.id);
    return LD_OK;
}

static int holiday_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return holiday_save(ctx, data, out, false); }
static int holiday_update(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return holiday_save(ctx, data, out, true); }

static int holiday_delete(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int id;
    if (!get_range(data, "id", 1, UINT16_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    ld_lock(g_cat_lock);
    int i = holiday_idx((uint16_t) id);
    if (i >= 0) REMOVE_AT(g_cat.holidays, g_cat.n_holidays, i);
    ld_unlock(g_cat_lock);
    if (i < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co ngay nghi nay", "id");
    changed("holiday", (uint32_t) id);
    return LD_OK;
}

// ---------- Môn học ----------

// subject.list → {subjects:[{id, code, name, credits}]}
static int subject_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    cJSON *arr = cJSON_AddArrayToObject(out, "subjects");
    ld_lock(g_cat_lock);
    for (int i = 0; i < g_cat.n_subjects; i++) {
        const cat_subject_t *s = &g_cat.subjects[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", s->id);
        cJSON_AddStringToObject(o, "code", s->code);
        cJSON_AddStringToObject(o, "name", s->name);
        cJSON_AddNumberToObject(o, "credits", s->credits);
        cJSON_AddItemToArray(arr, o);
    }
    ld_unlock(g_cat_lock);
    return LD_OK;
}

// subject.create {code, name, credits} → {id}; subject.update thêm id. Mã môn không trùng.
static int subject_save(const ld_ctx_t *ctx, const cJSON *data, cJSON *out, bool update)
{
    int id = 0, credits = 0;
    if (update && !get_range(data, "id", 1, UINT16_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    cat_subject_t s = {0};
    if (!take_str(ld_get_str(data, "code"), s.code, sizeof(s.code), true))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ma mon 1..16 ky tu", "code");
    if (!take_str(ld_get_str(data, "name"), s.name, sizeof(s.name), true))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ten mon 1..64 ky tu", "name");
    if (ld_get_int(data, "credits", &credits) && (credits < 0 || credits > 20))
        return ld_fail(out, LD_ERR_BAD_ARG, "So tin chi 0..20", "credits");
    s.credits = (uint8_t) credits;

    ld_lock(g_cat_lock);
    int code = LD_OK;
    for (int i = 0; i < g_cat.n_subjects; i++)
        if (g_cat.subjects[i].id != id && !strcmp(g_cat.subjects[i].code, s.code)) code = LD_ERR_CONFLICT;
    if (code == LD_OK) {
        if (update) {
            int i = subject_idx((uint16_t) id);
            if (i < 0) code = LD_ERR_NOT_FOUND;
            else {
                s.id = (uint16_t) id;
                g_cat.subjects[i] = s;
            }
        } else if (g_cat.n_subjects >= CAT_MAX_SUBJECTS || !(s.id = new_id())) {
            code = LD_ERR_FULL;
        } else {
            g_cat.subjects[g_cat.n_subjects++] = s;
        }
    }
    ld_unlock(g_cat_lock);
    if (code == LD_ERR_CONFLICT) return ld_fail(out, code, "Ma mon da ton tai", "code");
    if (code == LD_ERR_NOT_FOUND) return ld_fail(out, code, "Khong co mon nay", "id");
    if (code == LD_ERR_FULL) return ld_fail(out, code, "Da du so mon hoc", NULL);
    changed("subject", s.id);
    cJSON_AddNumberToObject(out, "id", s.id);
    return LD_OK;
}

static int subject_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return subject_save(ctx, data, out, false); }
static int subject_update(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return subject_save(ctx, data, out, true); }

// subject.delete {id}: còn lớp học phần của môn → CONFLICT
static int subject_delete(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int id;
    if (!get_range(data, "id", 1, UINT16_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    ld_lock(g_cat_lock);
    int i = subject_idx((uint16_t) id);
    bool used = false;
    for (int k = 0; k < g_cat.n_classes && !used; k++) used = g_cat.classes[k].subject_id == id;
    if (i >= 0 && !used) REMOVE_AT(g_cat.subjects, g_cat.n_subjects, i);
    ld_unlock(g_cat_lock);
    if (i < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co mon nay", "id");
    if (used) return ld_fail(out, LD_ERR_CONFLICT, "Mon con lop hoc phan, xoa lop truoc", "id");
    changed("subject", (uint32_t) id);
    return LD_OK;
}

// ---------- Lớp / nhóm học phần ----------

// class.list {schoolId?} → {classes:[{id, code, subjectId, schoolId, size}]}
static int class_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int sid = 0;
    ld_get_int(data, "schoolId", &sid);
    cJSON *arr = cJSON_AddArrayToObject(out, "classes");
    ld_lock(g_cat_lock);
    for (int i = 0; i < g_cat.n_classes; i++) {
        const cat_class_t *c = &g_cat.classes[i];
        if (sid > 0 && c->school_id != sid) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", c->id);
        cJSON_AddStringToObject(o, "code", c->code);
        cJSON_AddNumberToObject(o, "subjectId", c->subject_id);
        cJSON_AddNumberToObject(o, "schoolId", c->school_id);
        cJSON_AddNumberToObject(o, "size", c->size);
        cJSON_AddItemToArray(arr, o);
    }
    ld_unlock(g_cat_lock);
    return LD_OK;
}

// class.create {code, subjectId, schoolId, size} → {id}; class.update thêm id
static int class_save(const ld_ctx_t *ctx, const cJSON *data, cJSON *out, bool update)
{
    int id = 0, subj, sid, size = 0;
    if (update && !get_range(data, "id", 1, UINT16_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    cat_class_t c = {0};
    if (!take_str(ld_get_str(data, "code"), c.code, sizeof(c.code), true))
        return ld_fail(out, LD_ERR_BAD_ARG, "Ma lop 1..16 ky tu", "code");
    if (!get_range(data, "subjectId", 1, UINT16_MAX, &subj)) return ld_fail(out, LD_ERR_BAD_ARG, "Chon mon hoc", "subjectId");
    if (!get_range(data, "schoolId", 1, UINT16_MAX, &sid)) return ld_fail(out, LD_ERR_BAD_ARG, "Chon truong", "schoolId");
    if (ld_get_int(data, "size", &size) && (size < 0 || size > 1000))
        return ld_fail(out, LD_ERR_BAD_ARG, "Si so 0..1000", "size");
    c.subject_id = (uint16_t) subj;
    c.school_id = (uint16_t) sid;
    c.size = (uint16_t) size;

    ld_lock(g_cat_lock);
    int code = LD_OK;
    const char *field = NULL;
    if (subject_idx(c.subject_id) < 0) {
        code = LD_ERR_NOT_FOUND;
        field = "subjectId";
    } else if (school_idx(c.school_id) < 0) {
        code = LD_ERR_NOT_FOUND;
        field = "schoolId";
    } else if (update) {
        int i = class_idx((uint16_t) id);
        if (i < 0) {
            code = LD_ERR_NOT_FOUND;
            field = "id";
        } else {
            c.id = (uint16_t) id;
            g_cat.classes[i] = c;
        }
    } else if (g_cat.n_classes >= CAT_MAX_CLASSES || !(c.id = new_id())) {
        code = LD_ERR_FULL;
    } else {
        g_cat.classes[g_cat.n_classes++] = c;
    }
    ld_unlock(g_cat_lock);
    if (code == LD_ERR_NOT_FOUND) return ld_fail(out, code, "Khong tim thay", field);
    if (code == LD_ERR_FULL) return ld_fail(out, code, "Da du so lop", NULL);
    changed("class", c.id);
    cJSON_AddNumberToObject(out, "id", c.id);
    return LD_OK;
}

static int class_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return class_save(ctx, data, out, false); }
static int class_update(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return class_save(ctx, data, out, true); }

// class.delete {id}: lịch dạy đang dùng lớp → CONFLICT
static int class_delete(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int id;
    if (!get_range(data, "id", 1, UINT16_MAX, &id)) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu id", "id");
    if (sched_uses_class((uint16_t) id)) return ld_fail(out, LD_ERR_CONFLICT, "Lop dang duoc lich day su dung", "id");
    ld_lock(g_cat_lock);
    int i = class_idx((uint16_t) id);
    if (i >= 0) REMOVE_AT(g_cat.classes, g_cat.n_classes, i);
    ld_unlock(g_cat_lock);
    if (i < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Khong co lop nay", "id");
    changed("class", (uint32_t) id);
    return LD_OK;
}

// ---------- Thời gian di chuyển (hai chiều như nhau) ----------

// travel.get → {travel:[{campusA, campusB, minutes}], defaultTravel}
static int travel_get(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    cJSON *arr = cJSON_AddArrayToObject(out, "travel");
    ld_lock(g_cat_lock);
    for (int i = 0; i < g_cat.n_travel; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "campusA", g_cat.travel[i].campus_a);
        cJSON_AddNumberToObject(o, "campusB", g_cat.travel[i].campus_b);
        cJSON_AddNumberToObject(o, "minutes", g_cat.travel[i].minutes);
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddNumberToObject(out, "defaultTravel", g_cat.default_travel);
    ld_unlock(g_cat_lock);
    return LD_OK;
}

// travel.set {campusA, campusB, minutes}: thêm hoặc sửa cặp cơ sở
static int travel_set(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int a, b, m;
    if (!get_range(data, "campusA", 1, UINT16_MAX, &a)) return ld_fail(out, LD_ERR_BAD_ARG, "Chon co so di", "campusA");
    if (!get_range(data, "campusB", 1, UINT16_MAX, &b)) return ld_fail(out, LD_ERR_BAD_ARG, "Chon co so den", "campusB");
    if (a == b) return ld_fail(out, LD_ERR_BAD_ARG, "Hai co so phai khac nhau", "campusB");
    if (!get_range(data, "minutes", 0, MAX_TRAVEL_MINUTES, &m))
        return ld_fail(out, LD_ERR_BAD_ARG, "So phut 0..600", "minutes");
    ld_lock(g_cat_lock);
    int code = LD_OK;
    if (campus_idx((uint16_t) a) < 0 || campus_idx((uint16_t) b) < 0) {
        code = LD_ERR_NOT_FOUND;
    } else {
        int i = travel_idx((uint16_t) a, (uint16_t) b);
        if (i < 0) {
            if (g_cat.n_travel >= CAT_MAX_TRAVEL) code = LD_ERR_FULL;
            else i = g_cat.n_travel++;
        }
        if (i >= 0) g_cat.travel[i] = (cat_travel_t) {.campus_a = (uint16_t) a, .campus_b = (uint16_t) b, .minutes = (uint16_t) m};
    }
    ld_unlock(g_cat_lock);
    if (code == LD_ERR_NOT_FOUND) return ld_fail(out, code, "Khong co co so nay", "campusA");
    if (code == LD_ERR_FULL) return ld_fail(out, code, "Da du so cap co so", NULL);
    changed("travel", 0);
    return LD_OK;
}

// travel.delete {campusA, campusB}
static int travel_delete(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int a, b;
    if (!get_range(data, "campusA", 1, UINT16_MAX, &a) || !get_range(data, "campusB", 1, UINT16_MAX, &b))
        return ld_fail(out, LD_ERR_BAD_ARG, "Thieu campusA, campusB", "campusA");
    ld_lock(g_cat_lock);
    int i = travel_idx((uint16_t) a, (uint16_t) b);
    if (i >= 0) REMOVE_AT(g_cat.travel, g_cat.n_travel, i);
    ld_unlock(g_cat_lock);
    if (i < 0) return ld_fail(out, LD_ERR_NOT_FOUND, "Chua co thoi gian cho cap nay", NULL);
    changed("travel", 0);
    return LD_OK;
}

// travel.setDefault {minutes}: thời gian dùng cho cặp cơ sở chưa khai báo
static int travel_set_default(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int m;
    if (!get_range(data, "minutes", 0, MAX_TRAVEL_MINUTES, &m)) return ld_fail(out, LD_ERR_BAD_ARG, "So phut 0..600", "minutes");
    ld_lock(g_cat_lock);
    g_cat.default_travel = (uint16_t) m;
    ld_unlock(g_cat_lock);
    changed("travel", 0);
    return LD_OK;
}

void catalog_register_cmds(void)
{
    ld_register_cmd("school.list", school_list, LD_ROLE_STUDENT);
    ld_register_cmd("school.create", school_create, LD_ROLE_TEACHER);
    ld_register_cmd("school.update", school_update, LD_ROLE_TEACHER);
    ld_register_cmd("school.delete", school_delete, LD_ROLE_TEACHER);
    ld_register_cmd("periods.set", periods_set, LD_ROLE_TEACHER);
    ld_register_cmd("campus.list", campus_list, LD_ROLE_STUDENT);
    ld_register_cmd("campus.create", campus_create, LD_ROLE_TEACHER);
    ld_register_cmd("campus.update", campus_update, LD_ROLE_TEACHER);
    ld_register_cmd("campus.delete", campus_delete, LD_ROLE_TEACHER);
    ld_register_cmd("holiday.list", holiday_list, LD_ROLE_STUDENT);
    ld_register_cmd("holiday.create", holiday_create, LD_ROLE_TEACHER);
    ld_register_cmd("holiday.update", holiday_update, LD_ROLE_TEACHER);
    ld_register_cmd("holiday.delete", holiday_delete, LD_ROLE_TEACHER);
    ld_register_cmd("subject.list", subject_list, LD_ROLE_STUDENT);
    ld_register_cmd("subject.create", subject_create, LD_ROLE_TEACHER);
    ld_register_cmd("subject.update", subject_update, LD_ROLE_TEACHER);
    ld_register_cmd("subject.delete", subject_delete, LD_ROLE_TEACHER);
    ld_register_cmd("class.list", class_list, LD_ROLE_STUDENT);
    ld_register_cmd("class.create", class_create, LD_ROLE_TEACHER);
    ld_register_cmd("class.update", class_update, LD_ROLE_TEACHER);
    ld_register_cmd("class.delete", class_delete, LD_ROLE_TEACHER);
    ld_register_cmd("travel.get", travel_get, LD_ROLE_STUDENT);
    ld_register_cmd("travel.set", travel_set, LD_ROLE_TEACHER);
    ld_register_cmd("travel.delete", travel_delete, LD_ROLE_TEACHER);
    ld_register_cmd("travel.setDefault", travel_set_default, LD_ROLE_TEACHER);
}
