
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
    *out = v->valueint;
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
    ctx.role = commands[i].role;

    int result = commands[i].fn(&ctx, data, out);
    cJSON_Delete(data);
    return result;
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

    CHECK_EQ(role_of("schedule.create"), LD_ROLE_TEACHER);
    CHECK_EQ(role_of("schedule.update"), LD_ROLE_TEACHER);
    CHECK_EQ(role_of("schedule.delete"), LD_ROLE_TEACHER);
    CHECK_EQ(role_of("task.create"), LD_ROLE_STUDENT);
    CHECK_EQ(role_of("task.delete"), LD_ROLE_STUDENT);
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

int main(void)
{
    RUN(command_roles_are_registered);
    RUN(create_schedule_sets_owner_and_notifies);
    RUN(other_user_cannot_update_schedule);
    RUN(other_user_cannot_delete_schedule);
    RUN(invalid_schedule_does_not_persist);
    RUN(create_task_sets_owner_and_notifies);
    RUN(other_user_cannot_delete_task);
    RUN(deadline_task_accepts_equal_start_and_end);
    RUN(conflict_blocks_schedule_without_force);
    RUN(force_allows_schedule_conflict);
    return REPORT();
}
