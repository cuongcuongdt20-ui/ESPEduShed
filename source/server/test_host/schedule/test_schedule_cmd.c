
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "test.h"
#include "platform.h"
#include "schedule.h"
#include "schedule_internal.h"
#include "catalog.h"
#include "conflict.h"
#include "cJSON.h"

/* Bảng đăng ký command giả để kiểm tra tên, handler và role. */
typedef struct {
    char name[40];
    ld_cmd_fn fn;
    uint8_t role;
} registered_cmd_t;

static registered_cmd_t commands[32];
static int command_count;
static int dirty_count;
static int push_count;
static uint16_t last_push_user;
static char last_push_entity[32];
static uint32_t last_push_id;
static int fake_conflicts;

void ld_register_cmd(const char *name, ld_cmd_fn fn, uint8_t min_role)
{
    if (command_count >= 32) return;
    snprintf(commands[command_count].name,
             sizeof(commands[command_count].name), "%s", name);
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

void ld_push_changed_user(uint16_t user_id, const char *entity, uint32_t id)
{
    push_count++;
    last_push_user = user_id;
    snprintf(last_push_entity, sizeof(last_push_entity), "%s", entity);
    last_push_id = id;
}

void ld_push_changed(const char *entity, uint32_t id)
{
    ld_push_changed_user(0, entity, id);
}

/* Stub persistence notification: không ghi file trong test host. */
void schedule_mark_dirty(void)
{
    dirty_count++;
}

/* Stub xung đột có thể bật/tắt trong từng test. */
int conflict_check(uint16_t user_id, const occurrence_t *candidates,
                   int n, uint32_t ignore_source_id,
                   conflict_t *out, int max)
{
    (void)user_id;
    (void)ignore_source_id;

    if (!fake_conflicts || n <= 0 || max <= 0) return 0;

    memset(&out[0], 0, sizeof(out[0]));
    out[0].a = candidates[0];
    out[0].b = candidates[0];
    out[0].b.source_id = 999;
    out[0].b.kind = OCC_TEACH;
    out[0].reason = CONFLICT_OVERLAP;
    out[0].overlap_start = candidates[0].start_min;
    out[0].overlap_end = candidates[0].end_min;
    return 1;
}

static void reset_fixture(void)
{
    schedule_core_init();
    command_count = 0;
    dirty_count = 0;
    push_count = 0;
    last_push_user = 0;
    last_push_entity[0] = '\0';
    last_push_id = 0;
    fake_conflicts = 0;
    schedule_register_cmds();
}

static int find_command(const char *name)
{
    for (int i = 0; i < command_count; i++) {
        if (strcmp(commands[i].name, name) == 0) return i;
    }
    return -1;
}

static int role_of(const char *name)
{
    int i = find_command(name);
    return i < 0 ? -1 : commands[i].role;
}

static int invoke(const char *name, uint16_t user_id,
                  const char *json, cJSON *out)
{
    int i = find_command(name);
    if (i < 0) return -999;

    cJSON *data = cJSON_Parse(json);
    if (!data) return -998;

    ld_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.user_id = user_id;

    int result = commands[i].fn(&ctx, data, out);
    cJSON_Delete(data);
    return result;
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

static const char *valid_schedule =
    "{\"subject_code\":\"CS101\","
    "\"subject_name\":\"Lap trinh\","
    "\"school_id\":1,"
    "\"date_from\":\"2026-11-02\","
    "\"date_to\":\"2026-11-09\","
    "\"day_of_week\":1,"
    "\"time_mode\":0,"
    "\"period_start\":3,"
    "\"period_count\":2}";

TEST(command_roles_are_registered)
{
    reset_fixture();

    CHECK_EQ(role_of("schedule.create"), LD_ROLE_USER);
    CHECK_EQ(role_of("schedule.update"), LD_ROLE_USER);
    CHECK_EQ(role_of("schedule.delete"), LD_ROLE_USER);
    CHECK_EQ(role_of("task.create"), LD_ROLE_USER);
    CHECK_EQ(role_of("task.delete"), LD_ROLE_USER);
    CHECK(find_command("exception.set") >= 0);
    CHECK(find_command("occurrence.range") >= 0);
}

TEST(create_schedule_sets_owner_and_notifies)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();

    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out), LD_OK);
    CHECK_EQ(g_sched.n_series, 1);
    CHECK_EQ(g_sched.series[0].owner_id, 42);
    CHECK_EQ(dirty_count, 1);
    CHECK_EQ(push_count, 1);
    CHECK_EQ(last_push_user, 42);
    CHECK(strcmp(last_push_entity, "schedule") == 0);
    CHECK_EQ(last_push_id, g_sched.series[0].id);

    cJSON_Delete(out);
}

TEST(other_user_cannot_update_schedule)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();

    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out), LD_OK);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    const char *update =
        "{\"id\":1,\"subject_code\":\"CS102\","
        "\"subject_name\":\"Mon hoc khac\","
        "\"school_id\":1,\"date_from\":\"2026-11-02\","
        "\"date_to\":\"2026-11-09\",\"day_of_week\":1,"
        "\"time_mode\":0,\"period_start\":3,\"period_count\":2}";

    CHECK_EQ(invoke("schedule.update", 77, update, out), LD_ERR_NOT_FOUND);
    CHECK_EQ(g_sched.series[0].owner_id, 42);
    CHECK(strcmp(g_sched.series[0].subject_code, "CS101") == 0);

    cJSON_Delete(out);
}

TEST(other_user_cannot_delete_schedule)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();

    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out), LD_OK);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.delete", 77, "{\"id\":1}", out),
             LD_ERR_NOT_FOUND);
    CHECK_EQ(g_sched.n_series, 1);

    cJSON_Delete(out);
}

TEST(invalid_schedule_does_not_persist)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();

    CHECK(invoke("schedule.create", 42,
                 "{\"subject_code\":\"CS101\"}", out) != LD_OK);
    CHECK_EQ(g_sched.n_series, 0);
    CHECK_EQ(dirty_count, 0);
    CHECK_EQ(push_count, 0);

    cJSON_Delete(out);
}

TEST(invalid_optional_values_are_rejected)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.create", 42,
                    "{\"subject_code\":\"CS101\",\"subject_name\":\"Lap trinh\","
                    "\"school_id\":1,\"date_from\":\"2026-11-02\","
                    "\"date_to\":\"2026-11-09\",\"day_of_week\":1,"
                    "\"time_mode\":0,\"period_start\":3,\"period_count\":2,"
                    "\"credits\":99}", out),
             LD_ERR_BAD_ARG);
    CHECK(strcmp(item(out, "_field")->valuestring, "credits") == 0);
    CHECK_EQ(g_sched.n_series, 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("task.create", 15,
                    "{\"title\":\"X\",\"day\":\"2026-11-02\","
                    "\"start\":\"10:00\",\"type\":300}", out),
             LD_ERR_BAD_ARG);
    CHECK(strcmp(item(out, "_field")->valuestring, "type") == 0);
    CHECK_EQ(g_sched.n_tasks, 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("task.create", 15,
                    "{\"title\":\"X\",\"day\":\"2026-11-02\","
                    "\"start\":\"10:00\",\"campus_id\":-1}", out),
             LD_ERR_BAD_ARG);
    CHECK(strcmp(item(out, "_field")->valuestring, "campus_id") == 0);
    cJSON_Delete(out);
}

TEST(create_task_sets_owner_and_notifies)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    const char *task =
        "{\"title\":\"Nop bai\","
        "\"day\":\"2026-11-02\","
        "\"start\":\"10:00\",\"end\":\"10:30\"}";

    CHECK_EQ(invoke("task.create", 15, task, out), LD_OK);
    CHECK_EQ(g_sched.n_tasks, 1);
    CHECK_EQ(g_sched.tasks[0].owner_id, 15);
    CHECK_EQ(dirty_count, 1);
    CHECK_EQ(push_count, 1);
    CHECK_EQ(last_push_user, 15);
    CHECK(strcmp(last_push_entity, "task") == 0);

    cJSON_Delete(out);
}

TEST(other_user_cannot_delete_task)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    const char *task =
        "{\"title\":\"Nop bai\","
        "\"day\":\"2026-11-02\","
        "\"start\":\"10:00\",\"end\":\"10:30\"}";

    CHECK_EQ(invoke("task.create", 15, task, out), LD_OK);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("task.delete", 99, "{\"id\":1}", out),
             LD_ERR_NOT_FOUND);
    CHECK_EQ(g_sched.n_tasks, 1);

    cJSON_Delete(out);
}

TEST(deadline_task_accepts_equal_start_and_end)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    const char *task =
        "{\"title\":\"Han nop\","
        "\"day\":\"2026-11-02\","
        "\"start\":\"23:00\",\"deadline\":true}";

    CHECK_EQ(invoke("task.create", 15, task, out), LD_OK);
    CHECK_EQ(g_sched.n_tasks, 1);
    CHECK_EQ(g_sched.tasks[0].start_min, g_sched.tasks[0].end_min);

    cJSON_Delete(out);
}

TEST(conflict_blocks_schedule_without_force)
{
    reset_fixture();
    fake_conflicts = 1;
    cJSON *out = cJSON_CreateObject();

    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out),
             LD_ERR_CONFLICT);
    CHECK_EQ(g_sched.n_series, 0);
    CHECK_EQ(dirty_count, 0);

    cJSON_Delete(out);
}

TEST(force_allows_schedule_conflict)
{
    reset_fixture();
    fake_conflicts = 1;
    cJSON *out = cJSON_CreateObject();

    const char *forced =
        "{\"subject_code\":\"CS101\","
        "\"subject_name\":\"Lap trinh\","
        "\"school_id\":1,"
        "\"date_from\":\"2026-11-02\","
        "\"date_to\":\"2026-11-09\","
        "\"day_of_week\":1,"
        "\"time_mode\":0,"
        "\"period_start\":3,"
        "\"period_count\":2,"
        "\"force\":true}";

    CHECK_EQ(invoke("schedule.create", 42, forced, out), LD_OK);
    CHECK_EQ(g_sched.n_series, 1);
    CHECK_EQ(dirty_count, 1);

    cJSON_Delete(out);
}

TEST(schedule_get_returns_converted_period_times)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out), LD_OK);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.get", 42, "{\"id\":1}", out), LD_OK);
    const cJSON *s = item(out, "schedule");
    CHECK(cJSON_IsObject(s));
    CHECK(strcmp(item(s, "start")->valuestring, "07:10") == 0);   // catalog giả: tiết 3-4
    CHECK(strcmp(item(s, "end")->valuestring, "09:15") == 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.get", 77, "{\"id\":1}", out), LD_ERR_NOT_FOUND);
    cJSON_Delete(out);
}

TEST(occurrence_range_expands_real_schedule)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out), LD_OK);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("occurrence.range", 42,
                    "{\"from\":\"2026-11-02\",\"to\":\"2026-11-09\"}", out), LD_OK);
    CHECK_EQ(array_size(out, "items"), 2);        // hai thứ Hai
    const cJSON *first = cJSON_GetArrayItem(item(out, "items"), 0);
    CHECK_EQ(item(first, "sourceId")->valueint, 1);
    CHECK(strcmp(item(first, "date")->valuestring, "2026-11-02") == 0);
    CHECK(strcmp(item(first, "origDate")->valuestring, "2026-11-02") == 0);
    CHECK(strcmp(item(first, "start")->valuestring, "07:10") == 0);
    CHECK_EQ(item(first, "status")->valueint, OCC_NORMAL);
    cJSON_Delete(out);

    out = cJSON_CreateObject();                    // người khác không thấy lịch
    CHECK_EQ(invoke("occurrence.range", 77,
                    "{\"from\":\"2026-11-02\",\"to\":\"2026-11-09\"}", out), LD_OK);
    CHECK_EQ(array_size(out, "items"), 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();                    // quá 62 ngày
    CHECK_EQ(invoke("occurrence.range", 42,
                    "{\"from\":\"2026-01-01\",\"to\":\"2026-03-04\"}", out), LD_ERR_BAD_ARG);
    cJSON_Delete(out);

    out = cJSON_CreateObject();                    // thiếu from
    CHECK_EQ(invoke("occurrence.range", 42, "{\"to\":\"2026-11-09\"}", out), LD_ERR_BAD_ARG);
    CHECK(strcmp(item(out, "_field")->valuestring, "from") == 0);
    cJSON_Delete(out);
}

TEST(exception_cancel_and_delete)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out), LD_OK);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("exception.set", 42,
                    "{\"series_id\":1,\"orig_date\":\"2026-11-02\",\"type\":1}", out),
             LD_OK);
    CHECK_EQ(g_sched.n_exceptions, 1);
    CHECK_EQ(g_sched.exceptions[0].type, EXC_CANCEL);
    CHECK_EQ(push_count, 2);                       // create + exception.set
    CHECK(strcmp(last_push_entity, "exception") == 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("occurrence.range", 42,
                    "{\"from\":\"2026-11-02\",\"to\":\"2026-11-02\"}", out), LD_OK);
    CHECK_EQ(array_size(out, "items"), 1);
    CHECK_EQ(item(cJSON_GetArrayItem(item(out, "items"), 0), "status")->valueint, OCC_CANCELLED);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("exception.delete", 42,
                    "{\"series_id\":1,\"orig_date\":\"2026-11-02\"}", out), LD_OK);
    CHECK_EQ(g_sched.n_exceptions, 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("occurrence.range", 42,
                    "{\"from\":\"2026-11-02\",\"to\":\"2026-11-02\"}", out), LD_OK);
    CHECK_EQ(item(cJSON_GetArrayItem(item(out, "items"), 0), "status")->valueint, OCC_NORMAL);
    cJSON_Delete(out);
}

TEST(exception_rejects_foreign_series_and_bad_date)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out), LD_OK);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("exception.set", 77,
                    "{\"series_id\":1,\"orig_date\":\"2026-11-02\",\"type\":1}", out),
             LD_ERR_NOT_FOUND);
    CHECK_EQ(g_sched.n_exceptions, 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("exception.set", 42,
                    "{\"series_id\":1,\"orig_date\":\"2026-11-03\",\"type\":1}", out),
             LD_ERR_BAD_ARG);                      // thứ Ba không có buổi
    CHECK(strcmp(item(out, "_field")->valuestring, "orig_date") == 0);
    cJSON_Delete(out);
}

TEST(exception_move_emits_at_new_date)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out), LD_OK);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("exception.set", 42,
                    "{\"series_id\":1,\"orig_date\":\"2026-11-02\",\"type\":4,"
                    "\"new_date\":\"2026-11-03\"}", out),
             LD_ERR_BAD_ARG);                      // thiếu giờ
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("exception.set", 42,
                    "{\"series_id\":1,\"orig_date\":\"2026-11-02\",\"type\":4,"
                    "\"new_date\":\"2026-11-03\",\"new_start\":\"10:00\",\"new_end\":\"11:00\"}",
                    out),
             LD_OK);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("occurrence.range", 42,
                    "{\"from\":\"2026-11-02\",\"to\":\"2026-11-03\"}", out), LD_OK);
    CHECK_EQ(array_size(out, "items"), 1);        // 11-02 dời sang 11-03; 11-09 ngoài khoảng
    const cJSON *moved = cJSON_GetArrayItem(item(out, "items"), 0);
    CHECK(strcmp(item(moved, "date")->valuestring, "2026-11-03") == 0);
    CHECK(strcmp(item(moved, "origDate")->valuestring, "2026-11-02") == 0);
    CHECK(strcmp(item(moved, "start")->valuestring, "10:00") == 0);
    CHECK_EQ(item(moved, "status")->valueint, OCC_CHANGED);
    cJSON_Delete(out);
}

TEST(task_update_enforces_owner)
{
    reset_fixture();
    const char *task =
        "{\"title\":\"Nop bai\",\"day\":\"2026-11-02\","
        "\"start\":\"10:00\",\"end\":\"10:30\"}";
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("task.create", 15, task, out), LD_OK);
    cJSON_Delete(out);

    const char *update =
        "{\"id\":1,\"title\":\"Nop bai sua\",\"day\":\"2026-11-02\","
        "\"start\":\"10:00\",\"end\":\"10:30\",\"status\":2}";
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("task.update", 99, update, out), LD_ERR_NOT_FOUND);
    CHECK(strcmp(g_sched.tasks[0].title, "Nop bai") == 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("task.update", 15, update, out), LD_OK);
    CHECK(strcmp(g_sched.tasks[0].title, "Nop bai sua") == 0);
    CHECK_EQ(g_sched.tasks[0].status, 2);
    CHECK_EQ(g_sched.tasks[0].owner_id, 15);
    cJSON_Delete(out);
}

TEST(schedule_update_removes_stale_exceptions)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out), LD_OK);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("exception.set", 42,
                    "{\"series_id\":1,\"orig_date\":\"2026-11-09\",\"type\":1}", out),
             LD_OK);
    CHECK_EQ(g_sched.n_exceptions, 1);
    cJSON_Delete(out);

    const char *update =
        "{\"id\":1,\"subject_code\":\"CS101\",\"subject_name\":\"Lap trinh\","
        "\"school_id\":1,\"date_from\":\"2026-11-02\",\"date_to\":\"2026-11-02\","
        "\"day_of_week\":1,\"time_mode\":0,\"period_start\":3,\"period_count\":2}";
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.update", 42, update, out), LD_OK);
    CHECK_EQ(item(out, "removedExceptions")->valueint, 1);
    CHECK_EQ(g_sched.n_exceptions, 0);            // 11-09 không còn là buổi
    cJSON_Delete(out);
}

TEST(update_keeps_optional_fields_not_sent)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.create", 42,
                    "{\"subject_code\":\"CS101\",\"subject_name\":\"Lap trinh\","
                    "\"school_id\":1,\"date_from\":\"2026-11-02\","
                    "\"date_to\":\"2026-11-09\",\"day_of_week\":1,"
                    "\"time_mode\":0,\"period_start\":3,\"period_count\":2,"
                    "\"class_id\":5,\"credits\":3,\"size\":40}", out),
             LD_OK);
    cJSON_Delete(out);

    // Form sửa không gửi class_id, credits, size: phải giữ giá trị cũ, không về 0
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.update", 42,
                    "{\"id\":1,\"subject_code\":\"CS102\",\"subject_name\":\"Lap trinh\","
                    "\"school_id\":1,\"date_from\":\"2026-11-02\","
                    "\"date_to\":\"2026-11-09\",\"day_of_week\":1,"
                    "\"time_mode\":0,\"period_start\":3,\"period_count\":2}", out),
             LD_OK);
    CHECK(strcmp(g_sched.series[0].subject_code, "CS102") == 0);
    CHECK_EQ(g_sched.series[0].class_id, 5);
    CHECK_EQ(g_sched.series[0].credits, 3);
    CHECK_EQ(g_sched.series[0].size, 40);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("task.create", 15,
                    "{\"title\":\"Hop\",\"day\":\"2026-11-03\",\"start\":\"10:00\","
                    "\"end\":\"11:00\",\"campus_id\":2,\"status\":1}", out),
             LD_OK);
    uint32_t tid = (uint32_t) item(out, "id")->valueint;
    cJSON_Delete(out);

    char req[160];
    snprintf(req, sizeof(req),
             "{\"id\":%u,\"title\":\"Hop khoa\",\"day\":\"2026-11-03\",\"start\":\"10:00\",\"end\":\"11:30\"}",
             (unsigned) tid);
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("task.update", 15, req, out), LD_OK);
    CHECK_EQ(g_sched.tasks[0].campus_id, 2);
    CHECK_EQ(g_sched.tasks[0].status, 1);
    CHECK_EQ(g_sched.tasks[0].end_min, 11 * 60 + 30);
    cJSON_Delete(out);
}

TEST(fractional_optional_values_are_rejected)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.create", 42,
                    "{\"subject_code\":\"CS101\",\"subject_name\":\"Lap trinh\","
                    "\"school_id\":1,\"date_from\":\"2026-11-02\","
                    "\"date_to\":\"2026-11-09\",\"day_of_week\":1,"
                    "\"time_mode\":0,\"period_start\":3,\"period_count\":2,"
                    "\"credits\":2.5}", out),
             LD_ERR_BAD_ARG);
    const cJSON *field = item(out, "_field");
    CHECK(field && strcmp(field->valuestring, "credits") == 0);
    CHECK_EQ(g_sched.n_series, 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("task.create", 15,
                    "{\"title\":\"X\",\"day\":\"2026-11-02\",\"start\":\"10:00\","
                    "\"end\":\"11:00\",\"campus_id\":1.5}", out),
             LD_ERR_BAD_ARG);
    field = item(out, "_field");
    CHECK(field && strcmp(field->valuestring, "campus_id") == 0);
    CHECK_EQ(g_sched.n_tasks, 0);
    cJSON_Delete(out);
}

TEST(schedule_get_omits_time_when_periods_missing)
{
    reset_fixture();
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.create", 42, valid_schedule, out), LD_OK);
    cJSON_Delete(out);
    g_sched.series[0].first_period = 9;            // bảng tiết của trường không còn tiết 9-10

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("schedule.get", 42, "{\"id\":1}", out), LD_OK);
    const cJSON *s = item(out, "schedule");
    CHECK(cJSON_IsObject(s));
    CHECK(item(s, "start") == NULL);               // không trả 00:00
    CHECK(item(s, "end") == NULL);
    cJSON_Delete(out);
}

int main(void)
{
    RUN(command_roles_are_registered);
    RUN(create_schedule_sets_owner_and_notifies);
    RUN(other_user_cannot_update_schedule);
    RUN(other_user_cannot_delete_schedule);
    RUN(invalid_schedule_does_not_persist);
    RUN(invalid_optional_values_are_rejected);
    RUN(create_task_sets_owner_and_notifies);
    RUN(other_user_cannot_delete_task);
    RUN(deadline_task_accepts_equal_start_and_end);
    RUN(conflict_blocks_schedule_without_force);
    RUN(force_allows_schedule_conflict);
    RUN(schedule_get_returns_converted_period_times);
    RUN(occurrence_range_expands_real_schedule);
    RUN(exception_cancel_and_delete);
    RUN(exception_rejects_foreign_series_and_bad_date);
    RUN(exception_move_emits_at_new_date);
    RUN(task_update_enforces_owner);
    RUN(schedule_update_removes_stale_exceptions);
    RUN(update_keeps_optional_fields_not_sent);
    RUN(fractional_optional_values_are_rejected);
    RUN(schedule_get_omits_time_when_periods_missing);
    return REPORT();
}
