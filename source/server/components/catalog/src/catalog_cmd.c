// Lệnh TCP của danh mục (TV2). BẢN GIẢ do TV1 tạo khi chuyển sang giao thức TCP: school.list đã chạy,
// các lệnh khác trả lỗi "chưa làm". TV2 viết phần thật; tên lệnh và quyền theo README mục B2.
//
// Mỗi lệnh ghi phải: kiểm tra dữ liệu → sửa g_cat trong khoá → catalog_mark_dirty() → ld_push_changed().
#include "cJSON.h"
#include "platform.h"
#include "catalog.h"
#include "catalog_internal.h"
#include "dt.h"
#include "schedule.h"

static int not_yet(cJSON *out)
{
    return ld_fail(out, LD_ERR_INTERNAL, "Chua lam (TV2)", NULL);
}

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

// TODO(TV2): school.get/create/update/delete, campus.*, periods.set, holiday.*, subject.*, class.*, travel.*
//            xoá trường / lớp đang dùng (sched_uses_school, sched_uses_class) → LD_ERR_CONFLICT
static int school_create(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }
static int periods_set(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }
static int holiday_list(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }
static int travel_get(const ld_ctx_t *ctx, const cJSON *data, cJSON *out) { return not_yet(out); }

void catalog_register_cmds(void)
{
    ld_register_cmd("school.list", school_list, LD_ROLE_STUDENT);
    ld_register_cmd("school.create", school_create, LD_ROLE_TEACHER);
    ld_register_cmd("periods.set", periods_set, LD_ROLE_TEACHER);
    ld_register_cmd("holiday.list", holiday_list, LD_ROLE_STUDENT);
    ld_register_cmd("travel.get", travel_get, LD_ROLE_STUDENT);
}
