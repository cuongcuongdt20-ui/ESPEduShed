// Test host cho handler lệnh danh mục (TV2): school.*, periods.set, campus.*,
// holiday.*, subject.*, class.*, travel.*. Không cần ESP32: khoá là no-op,
// lưu trữ và lịch dạy được thay bằng stub.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "catalog.h"
#include "catalog_internal.h"
#include "platform.h"
#include "test.h"

/* ---------- Bảng đăng ký lệnh giả ---------- */

typedef struct {
    char name[40];
    ld_cmd_fn fn;
    uint8_t role;
} registered_cmd_t;

static registered_cmd_t commands[64];
static int command_count;
static int dirty_count;
static int push_count;
static char last_push_entity[32];
static uint32_t last_push_id;
static bool sched_uses_school_flag;
static bool sched_uses_class_flag;

void ld_register_cmd(const char *name, ld_cmd_fn fn, uint8_t min_role)
{
    if (command_count >= 64) return;
    snprintf(commands[command_count].name, sizeof(commands[command_count].name), "%s", name);
    commands[command_count].fn = fn;
    commands[command_count].role = min_role;
    command_count++;
}

int ld_fail(cJSON *out, int code, const char *message, const char *field)
{
    cJSON_AddNumberToObject(out, "_error", code);
    if (message) cJSON_AddStringToObject(out, "_message", message);
    if (field) cJSON_AddStringToObject(out, "_field", field);
    return code;
}

bool ld_get_int(const cJSON *data, const char *key, int *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(data, key);
    if (!cJSON_IsNumber(v)) return false;
    if (v->valuedouble != (double) (int) v->valuedouble) return false;   // giống protocol.c: không nhận số thực
    *out = (int) v->valuedouble;
    return true;
}

bool ld_get_bool(const cJSON *data, const char *key, bool *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(data, key);
    if (!cJSON_IsBool(v)) return false;
    *out = cJSON_IsTrue(v);
    return true;
}

const char *ld_get_str(const cJSON *data, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(data, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

int ld_page_offset(const cJSON *data)
{
    int offset = 0;
    if (ld_get_int(data, "offset", &offset) && offset > 0) return offset;
    return 0;
}

bool ld_page_add(cJSON *arr, cJSON *item, int *budget)
{
    if (!arr || !item || !budget) return false;
    char *text = cJSON_PrintUnformatted(item);
    int need = text ? (int) strlen(text) + 2 : 0;
    cJSON_free(text);
    if (need > *budget) {
        cJSON_Delete(item);
        return false;
    }
    *budget -= need;
    cJSON_AddItemToArray(arr, item);
    return true;
}

void ld_push_changed(const char *entity, uint32_t id)
{
    push_count++;
    snprintf(last_push_entity, sizeof(last_push_entity), "%s", entity ? entity : "");
    last_push_id = id;
}

void ld_push_changed_user(uint16_t user_id, const char *entity, uint32_t id)
{
    (void) user_id;
    ld_push_changed(entity, id);
}

/* Chỉ báo ghi file, không ghi thật trên máy tính. */
void catalog_mark_dirty(void)
{
    dirty_count++;
}

/* Stub quan hệ với lịch dạy (TV3): test tự bật/tắt. */
bool sched_uses_school(uint16_t school_id)
{
    (void) school_id;
    return sched_uses_school_flag;
}

bool sched_uses_class(uint16_t class_id)
{
    (void) class_id;
    return sched_uses_class_flag;
}

/* ---------- Fixture ---------- */

static void reset_fixture(void)
{
    catalog_core_init();
    command_count = 0;
    dirty_count = 0;
    push_count = 0;
    last_push_entity[0] = '\0';
    last_push_id = 0;
    sched_uses_school_flag = false;
    sched_uses_class_flag = false;
    catalog_register_cmds();
}

static int find_command(const char *name)
{
    for (int i = 0; i < command_count; i++)
        if (strcmp(commands[i].name, name) == 0) return i;
    return -1;
}

static int role_of(const char *name)
{
    int i = find_command(name);
    return i < 0 ? -1 : commands[i].role;
}

static int invoke(const char *name, const char *json, cJSON *out)
{
    int i = find_command(name);
    if (i < 0) return -999;

    cJSON *data = cJSON_Parse(json);
    if (!data) return -998;

    ld_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.user_id = 1;

    int result = commands[i].fn(&ctx, data, out);
    cJSON_Delete(data);
    return result;
}

/* Gọi lệnh không cần data. */
static int invoke_empty(const char *name, cJSON *out)
{
    return invoke(name, "{}", out);
}

static int out_id(const cJSON *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(out, "id");
    return cJSON_IsNumber(v) ? v->valueint : -1;
}

static const cJSON *item(const cJSON *obj, const char *key)
{
    return cJSON_GetObjectItemCaseSensitive(obj, key);
}

static int array_size(const cJSON *obj, const char *key)
{
    const cJSON *arr = item(obj, key);
    return cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : -1;
}

/* Tạo một trường hợp lệ, trả id (0 nếu lỗi). */
static int create_school(const char *name, const char *short_name)
{
    char json[256];
    snprintf(json, sizeof(json), "{\"name\":\"%s\",\"shortName\":\"%s\",\"color\":\"#2E7D32\"}",
             name, short_name);
    cJSON *out = cJSON_CreateObject();
    int rc = invoke("school.create", json, out);
    int id = rc == LD_OK ? out_id(out) : 0;
    cJSON_Delete(out);
    return id;
}

static int create_subject(const char *code)
{
    char json[256];
    snprintf(json, sizeof(json), "{\"code\":\"%s\",\"name\":\"Mon %s\",\"credits\":3}", code, code);
    cJSON *out = cJSON_CreateObject();
    int rc = invoke("subject.create", json, out);
    int id = rc == LD_OK ? out_id(out) : 0;
    cJSON_Delete(out);
    return id;
}

/* ---------- Đăng ký lệnh ---------- */

TEST(all_commands_registered_with_roles)
{
    reset_fixture();

    static const struct { const char *name; uint8_t role; } expected[] = {
        {"school.list", LD_ROLE_USER},
        {"school.create", LD_ROLE_USER},
        {"school.update", LD_ROLE_USER},
        {"school.delete", LD_ROLE_USER},
        {"periods.set", LD_ROLE_USER},
        {"campus.list", LD_ROLE_USER},
        {"campus.create", LD_ROLE_USER},
        {"campus.update", LD_ROLE_USER},
        {"campus.delete", LD_ROLE_USER},
        {"holiday.list", LD_ROLE_USER},
        {"holiday.create", LD_ROLE_USER},
        {"holiday.update", LD_ROLE_USER},
        {"holiday.delete", LD_ROLE_USER},
        {"subject.list", LD_ROLE_USER},
        {"subject.create", LD_ROLE_USER},
        {"subject.update", LD_ROLE_USER},
        {"subject.delete", LD_ROLE_USER},
        {"class.list", LD_ROLE_USER},
        {"class.create", LD_ROLE_USER},
        {"class.update", LD_ROLE_USER},
        {"class.delete", LD_ROLE_USER},
        {"travel.get", LD_ROLE_USER},
        {"travel.set", LD_ROLE_USER},
        {"travel.delete", LD_ROLE_USER},
        {"travel.setDefault", LD_ROLE_USER},
    };

    CHECK_EQ(command_count, (int) (sizeof(expected) / sizeof(expected[0])));
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
        CHECK_EQ(role_of(expected[i].name), expected[i].role);
}

/* ---------- Trường ---------- */

TEST(school_create_then_list)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();

    CHECK_EQ(invoke("school.create",
                    "{\"name\":\"Truong A\",\"shortName\":\"A\",\"color\":\"#2E7D32\"}", out),
             LD_OK);
    int id = out_id(out);
    CHECK(id > 0);
    CHECK_EQ(g_cat.n_schools, 1);
    CHECK_EQ(g_cat.schools[0].id, id);
    CHECK(strcmp(g_cat.schools[0].name, "Truong A") == 0);
    CHECK_EQ(dirty_count, 1);
    CHECK_EQ(push_count, 1);
    CHECK(strcmp(last_push_entity, "school") == 0);
    CHECK_EQ(last_push_id, (uint32_t) id);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke_empty("school.list", out), LD_OK);
    CHECK_EQ(array_size(out, "schools"), 1);
    const cJSON *s = cJSON_GetArrayItem(item(out, "schools"), 0);
    CHECK(cJSON_IsString(item(s, "shortName")));
    CHECK(strcmp(item(s, "shortName")->valuestring, "A") == 0);
    CHECK_EQ(array_size(s, "periods"), 0);
    cJSON_Delete(out);
}

TEST(school_create_defaults_color_and_rejects_bad_input)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();

    CHECK_EQ(invoke("school.create", "{\"name\":\"Truong B\",\"shortName\":\"B\"}", out), LD_OK);
    CHECK(strcmp(g_cat.schools[0].color, "#2563EB") == 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("school.create",
                    "{\"name\":\"Truong C\",\"shortName\":\"C\",\"color\":\"#12345\"}", out),
             LD_ERR_BAD_ARG);
    CHECK_EQ(g_cat.n_schools, 1);
    CHECK_EQ(dirty_count, 1);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("school.create", "{\"shortName\":\"D\"}", out), LD_ERR_BAD_ARG);
    CHECK_EQ(g_cat.n_schools, 1);
    cJSON_Delete(out);
}

TEST(school_update_keeps_periods)
{
    reset_fixture();
    int id = create_school("Truong A", "A");
    CHECK(id > 0);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("periods.set",
                    "{\"schoolId\":1,\"periods\":["
                    "{\"no\":2,\"start\":\"09:00\",\"end\":\"10:00\"},"
                    "{\"no\":1,\"start\":\"07:00\",\"end\":\"08:00\"}]}",
                    out),
             LD_OK);
    CHECK_EQ(g_cat.schools[0].n_periods, 2);
    CHECK_EQ(g_cat.schools[0].periods[0].no, 1);   // sắp theo giờ bắt đầu
    CHECK_EQ(g_cat.schools[0].periods[1].no, 2);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("school.update",
                    "{\"id\":1,\"name\":\"Truong A2\",\"shortName\":\"A2\",\"color\":\"#1565C0\"}",
                    out),
             LD_OK);
    CHECK(strcmp(g_cat.schools[0].name, "Truong A2") == 0);
    CHECK_EQ(g_cat.schools[0].n_periods, 2);       // không mất bảng tiết
    CHECK_EQ(dirty_count, 3);                      // tao truong + periods.set + update
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("school.update",
                    "{\"id\":999,\"name\":\"X\",\"shortName\":\"X\"}", out),
             LD_ERR_NOT_FOUND);
    CHECK_EQ(g_cat.n_schools, 1);
    cJSON_Delete(out);
}

TEST(periods_set_rejects_duplicate_and_overlap)
{
    reset_fixture();
    CHECK(create_school("Truong A", "A") > 0);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("periods.set",
                    "{\"schoolId\":1,\"periods\":["
                    "{\"no\":1,\"start\":\"07:00\",\"end\":\"08:00\"},"
                    "{\"no\":1,\"start\":\"09:00\",\"end\":\"10:00\"}]}",
                    out),
             LD_ERR_BAD_ARG);
    CHECK_EQ(g_cat.schools[0].n_periods, 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("periods.set",
                    "{\"schoolId\":1,\"periods\":["
                    "{\"no\":1,\"start\":\"07:00\",\"end\":\"08:30\"},"
                    "{\"no\":2,\"start\":\"08:00\",\"end\":\"09:00\"}]}",
                    out),
             LD_ERR_BAD_ARG);
    CHECK_EQ(g_cat.schools[0].n_periods, 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("periods.set",
                    "{\"schoolId\":1,\"periods\":["
                    "{\"no\":1,\"start\":\"08:00\",\"end\":\"09:00\"},"
                    "{\"no\":3,\"start\":\"07:00\",\"end\":\"08:00\"}]}",
                    out),
             LD_OK);                                // tiết số không cần liên tục
    CHECK_EQ(g_cat.schools[0].n_periods, 2);
    cJSON_Delete(out);
}

TEST(school_delete_cascades_and_blocks_when_used)
{
    reset_fixture();
    CHECK(create_school("Truong A", "A") > 0);
    CHECK(create_school("Truong B", "B") > 0);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("campus.create", "{\"schoolId\":1,\"name\":\"Co so A\"}", out), LD_OK);
    int campus_a = out_id(out);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("campus.create", "{\"schoolId\":2,\"name\":\"Co so B\"}", out), LD_OK);
    int campus_b = out_id(out);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("holiday.create",
                    "{\"schoolId\":1,\"name\":\"Nghi A\",\"from\":\"2026-09-01\",\"to\":\"2026-09-02\"}",
                    out),
             LD_OK);
    CHECK_EQ(g_cat.n_holidays, 1);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("holiday.create",
                    "{\"schoolId\":0,\"name\":\"Nghi chung\",\"from\":\"2026-09-03\",\"to\":\"2026-09-03\"}",
                    out),
             LD_OK);
    CHECK_EQ(g_cat.n_holidays, 2);
    cJSON_Delete(out);

    char travel[160];
    snprintf(travel, sizeof(travel),
             "{\"campusA\":%d,\"campusB\":%d,\"minutes\":25}", campus_a, campus_b);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("travel.set", travel, out), LD_OK);
    CHECK_EQ(g_cat.n_travel, 1);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("school.delete", "{\"id\":1}", out), LD_OK);
    CHECK_EQ(g_cat.n_schools, 1);
    CHECK_EQ(g_cat.schools[0].id, 2);
    CHECK_EQ(g_cat.n_campuses, 1);
    CHECK_EQ(g_cat.campuses[0].id, campus_b);
    CHECK_EQ(g_cat.n_holidays, 1);                 // ngày nghỉ chung còn lại
    CHECK_EQ(g_cat.holidays[0].school_id, 0);
    CHECK_EQ(g_cat.n_travel, 0);                   // cặp có cơ sở vừa xoá bị bỏ
    CHECK(strcmp(last_push_entity, "school") == 0);
    cJSON_Delete(out);

    // Lịch dạy đang dùng trường 2 → từ chối, không xoá gì.
    sched_uses_school_flag = true;
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("school.delete", "{\"id\":2}", out), LD_ERR_CONFLICT);
    CHECK_EQ(g_cat.n_schools, 1);
    CHECK_EQ(g_cat.n_campuses, 1);
    cJSON_Delete(out);
}

TEST(school_delete_blocked_when_classes_exist)
{
    reset_fixture();
    CHECK(create_school("Truong A", "A") > 0);
    int subject = create_subject("CS101");
    CHECK(subject > 0);

    char json[160];
    snprintf(json, sizeof(json),
             "{\"code\":\"CS101-01\",\"subjectId\":%d,\"schoolId\":1,\"size\":40}", subject);
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("class.create", json, out), LD_OK);
    CHECK_EQ(g_cat.n_classes, 1);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("school.delete", "{\"id\":1}", out), LD_ERR_CONFLICT);
    CHECK_EQ(g_cat.n_schools, 1);
    CHECK_EQ(g_cat.n_classes, 1);
    cJSON_Delete(out);
}

/* ---------- Cơ sở ---------- */

TEST(campus_crud_and_list_filter)
{
    reset_fixture();
    CHECK(create_school("Truong A", "A") > 0);
    CHECK(create_school("Truong B", "B") > 0);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("campus.create",
                    "{\"schoolId\":1,\"name\":\"Co so 1\",\"address\":\"Ha Noi\"}", out),
             LD_OK);
    int id1 = out_id(out);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("campus.create", "{\"schoolId\":2,\"name\":\"Co so 2\"}", out), LD_OK);
    int id2 = out_id(out);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("campus.list", "{\"schoolId\":1}", out), LD_OK);
    CHECK_EQ(array_size(out, "campuses"), 1);
    CHECK_EQ(item(cJSON_GetArrayItem(item(out, "campuses"), 0), "id")->valueint, id1);
    cJSON_Delete(out);

    char json[160];
    snprintf(json, sizeof(json),
             "{\"id\":%d,\"schoolId\":1,\"name\":\"Co so 3\",\"address\":\"Da Nang\"}", id2);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("campus.update", json, out), LD_OK);
    CHECK_EQ(g_cat.n_campuses, 2);
    CHECK_EQ(g_cat.campuses[1].school_id, 1);
    CHECK(strcmp(g_cat.campuses[1].address, "Da Nang") == 0);
    cJSON_Delete(out);

    snprintf(json, sizeof(json), "{\"id\":%d}", id1);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("campus.delete", json, out), LD_OK);
    CHECK_EQ(g_cat.n_campuses, 1);
    CHECK_EQ(g_cat.campuses[0].id, id2);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("campus.create", "{\"schoolId\":99,\"name\":\"X\"}", out), LD_ERR_NOT_FOUND);
    CHECK_EQ(g_cat.n_campuses, 1);
    cJSON_Delete(out);
}

/* ---------- Ngày nghỉ ---------- */

TEST(holiday_crud_and_filter)
{
    reset_fixture();
    CHECK(create_school("Truong A", "A") > 0);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("holiday.create",
                    "{\"schoolId\":1,\"name\":\"Nghi le\",\"from\":\"2026-09-02\",\"to\":\"2026-09-04\"}",
                    out),
             LD_OK);
    int specific = out_id(out);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("holiday.create",
                    "{\"name\":\"Toan quoc\",\"from\":\"2026-09-01\",\"to\":\"2026-09-01\"}", out),
             LD_OK);
    int global = out_id(out);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("holiday.list", "{\"schoolId\":1}", out), LD_OK);
    CHECK_EQ(array_size(out, "holidays"), 2);       // riêng của trường + toàn quốc
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("holiday.list", "{\"schoolId\":2}", out), LD_OK);
    CHECK_EQ(array_size(out, "holidays"), 1);       // chỉ còn ngày nghỉ toàn quốc
    cJSON_Delete(out);

    char json[192];
    snprintf(json, sizeof(json),
             "{\"id\":%d,\"schoolId\":1,\"name\":\"Nghi le dai\",\"from\":\"2026-09-02\",\"to\":\"2026-09-05\"}",
             specific);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("holiday.update", json, out), LD_OK);
    CHECK_EQ(g_cat.holidays[0].to - g_cat.holidays[0].from, 3);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("holiday.create",
                    "{\"schoolId\":1,\"name\":\"Sai\",\"from\":\"2026-09-05\",\"to\":\"2026-09-02\"}",
                    out),
             LD_ERR_BAD_ARG);
    CHECK_EQ(g_cat.n_holidays, 2);
    cJSON_Delete(out);

    snprintf(json, sizeof(json), "{\"id\":%d}", specific);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("holiday.delete", json, out), LD_OK);
    CHECK_EQ(g_cat.n_holidays, 1);
    CHECK_EQ(g_cat.holidays[0].id, global);
    cJSON_Delete(out);

    CHECK(specific != global);
}

/* ---------- Môn học ---------- */

TEST(subject_code_unique_and_delete_blocked_by_class)
{
    reset_fixture();
    CHECK(create_school("Truong A", "A") > 0);
    int s1 = create_subject("CS101");
    CHECK(s1 > 0);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("subject.create", "{\"code\":\"CS101\",\"name\":\"Trung ma\"}", out),
             LD_ERR_CONFLICT);
    CHECK_EQ(g_cat.n_subjects, 1);
    cJSON_Delete(out);

    char json[160];
    snprintf(json, sizeof(json), "{\"id\":%d,\"code\":\"CS101\",\"name\":\"Ten moi\"}", s1);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("subject.update", json, out), LD_OK);   // giữ nguyên mã của chính nó
    CHECK(strcmp(g_cat.subjects[0].name, "Ten moi") == 0);
    cJSON_Delete(out);

    snprintf(json, sizeof(json),
             "{\"code\":\"CS101-01\",\"subjectId\":%d,\"schoolId\":1,\"size\":40}", s1);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("class.create", json, out), LD_OK);
    int cls = out_id(out);
    cJSON_Delete(out);
    CHECK(cls > 0);

    snprintf(json, sizeof(json), "{\"id\":%d}", cls);
    sched_uses_class_flag = true;
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("class.delete", json, out), LD_ERR_CONFLICT);
    CHECK_EQ(g_cat.n_classes, 1);
    cJSON_Delete(out);

    sched_uses_class_flag = false;
    snprintf(json, sizeof(json), "{\"id\":%d}", s1);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("subject.delete", json, out), LD_ERR_CONFLICT);
    CHECK_EQ(g_cat.n_subjects, 1);
    cJSON_Delete(out);

    snprintf(json, sizeof(json), "{\"id\":%d}", cls);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("class.delete", json, out), LD_OK);
    CHECK_EQ(g_cat.n_classes, 0);
    cJSON_Delete(out);

    snprintf(json, sizeof(json), "{\"id\":%d}", s1);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("subject.delete", json, out), LD_OK);
    CHECK_EQ(g_cat.n_subjects, 0);
    cJSON_Delete(out);
}

/* ---------- Lớp học phần ---------- */

TEST(class_requires_existing_subject_and_school)
{
    reset_fixture();
    CHECK(create_school("Truong A", "A") > 0);
    int subj = create_subject("CS101");
    CHECK(subj > 0);

    char json[160];
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("class.create",
                    "{\"code\":\"L1\",\"subjectId\":99,\"schoolId\":1}", out),
             LD_ERR_NOT_FOUND);
    CHECK(cJSON_IsString(item(out, "_field")));
    CHECK(strcmp(item(out, "_field")->valuestring, "subjectId") == 0);
    cJSON_Delete(out);

    snprintf(json, sizeof(json), "{\"code\":\"L1\",\"subjectId\":%d,\"schoolId\":99}", subj);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("class.create", json, out), LD_ERR_NOT_FOUND);
    CHECK(strcmp(item(out, "_field")->valuestring, "schoolId") == 0);
    CHECK_EQ(g_cat.n_classes, 0);
    cJSON_Delete(out);

    snprintf(json, sizeof(json),
             "{\"code\":\"L1\",\"subjectId\":%d,\"schoolId\":1,\"size\":1001}", subj);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("class.create", json, out), LD_ERR_BAD_ARG);
    cJSON_Delete(out);

    snprintf(json, sizeof(json),
             "{\"code\":\"L1\",\"subjectId\":%d,\"schoolId\":1,\"size\":45}", subj);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("class.create", json, out), LD_OK);
    CHECK_EQ(g_cat.classes[0].size, 45);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("class.list", "{\"schoolId\":1}", out), LD_OK);
    CHECK_EQ(array_size(out, "classes"), 1);
    CHECK_EQ(item(cJSON_GetArrayItem(item(out, "classes"), 0), "size")->valueint, 45);
    cJSON_Delete(out);
}

/* ---------- Thời gian di chuyển ---------- */

TEST(travel_is_symmetric_and_validated)
{
    reset_fixture();
    CHECK(create_school("Truong A", "A") > 0);
    CHECK(create_school("Truong B", "B") > 0);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("campus.create", "{\"schoolId\":1,\"name\":\"Co so 1\"}", out), LD_OK);
    int a = out_id(out);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("campus.create", "{\"schoolId\":2,\"name\":\"Co so 2\"}", out), LD_OK);
    int b = out_id(out);
    cJSON_Delete(out);

    char json[160];
    snprintf(json, sizeof(json), "{\"campusA\":%d,\"campusB\":%d,\"minutes\":25}", a, b);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("travel.set", json, out), LD_OK);
    CHECK_EQ(g_cat.n_travel, 1);
    CHECK_EQ(g_cat.travel[0].minutes, 25);
    CHECK_EQ(dirty_count, 5);                      // 2 truong + 2 co so + 1 travel
    cJSON_Delete(out);

    snprintf(json, sizeof(json), "{\"campusA\":%d,\"campusB\":%d,\"minutes\":40}", b, a);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("travel.set", json, out), LD_OK);
    CHECK_EQ(g_cat.n_travel, 1);                   // cặp đảo cập nhật cùng bản ghi
    CHECK_EQ(g_cat.travel[0].minutes, 40);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke_empty("travel.get", out), LD_OK);
    CHECK_EQ(array_size(out, "travel"), 1);
    CHECK_EQ(item(cJSON_GetArrayItem(item(out, "travel"), 0), "minutes")->valueint, 40);
    CHECK_EQ(item(out, "defaultTravel")->valueint, 30);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("travel.setDefault", "{\"minutes\":20}", out), LD_OK);
    CHECK_EQ(g_cat.default_travel, 20);
    cJSON_Delete(out);

    snprintf(json, sizeof(json), "{\"campusA\":%d,\"campusB\":%d,\"minutes\":10}", a, a);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("travel.set", json, out), LD_ERR_BAD_ARG);
    cJSON_Delete(out);

    snprintf(json, sizeof(json), "{\"campusA\":%d,\"campusB\":%d,\"minutes\":601}", a, b);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("travel.set", json, out), LD_ERR_BAD_ARG);
    cJSON_Delete(out);

    snprintf(json, sizeof(json), "{\"campusA\":%d,\"campusB\":%d,\"minutes\":15}", a, 99);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("travel.set", json, out), LD_ERR_NOT_FOUND);
    cJSON_Delete(out);

    snprintf(json, sizeof(json), "{\"campusA\":%d,\"campusB\":%d}", b, a);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("travel.delete", json, out), LD_OK);
    CHECK_EQ(g_cat.n_travel, 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("travel.delete", json, out), LD_ERR_NOT_FOUND);
    cJSON_Delete(out);
}

int main(void)
{
    RUN(all_commands_registered_with_roles);
    RUN(school_create_then_list);
    RUN(school_create_defaults_color_and_rejects_bad_input);
    RUN(school_update_keeps_periods);
    RUN(periods_set_rejects_duplicate_and_overlap);
    RUN(school_delete_cascades_and_blocks_when_used);
    RUN(school_delete_blocked_when_classes_exist);
    RUN(campus_crud_and_list_filter);
    RUN(holiday_crud_and_filter);
    RUN(subject_code_unique_and_delete_blocked_by_class);
    RUN(class_requires_existing_subject_and_school);
    RUN(travel_is_symmetric_and_validated);
    return REPORT();
}
