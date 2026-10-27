// Test host cho handler lệnh lịch/xung đột (TV4): calendar.week, calendar.range,
// conflict.list, conflict.check, conflict.ack. Dùng conflict.c thật; thay
// sched_expand/sched_label, thời gian hiện tại và lưu xác nhận bằng stub.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "calendar_internal.h"
#include "conflict.h"
#include "dt.h"
#include "platform.h"
#include "schedule.h"
#include "test.h"

/* ---------- Bảng đăng ký lệnh giả ---------- */

typedef struct {
    char name[40];
    ld_cmd_fn fn;
    uint8_t role;
} registered_cmd_t;

static registered_cmd_t commands[16];
static int command_count;
static int push_count;
static uint16_t last_push_user;
static char last_push_entity[32];

static occurrence_t fake_occ[32];
static int fake_n_occ;
static int expand_calls;
static bool expand_overflow;
static int32_t fake_today;

void ld_register_cmd(const char *name, ld_cmd_fn fn, uint8_t min_role)
{
    if (command_count >= 16) return;
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

void ld_push_changed(const char *entity, uint32_t id)
{
    (void) id;
    push_count++;
    last_push_user = 0;
    snprintf(last_push_entity, sizeof(last_push_entity), "%s", entity ? entity : "");
}

void ld_push_changed_user(uint16_t user_id, const char *entity, uint32_t id)
{
    (void) id;
    push_count++;
    last_push_user = user_id;
    snprintf(last_push_entity, sizeof(last_push_entity), "%s", entity ? entity : "");
}

/* ---------- Stub TV3, thời gian, lưu xác nhận ---------- */

/* Trả các buổi giả nằm trong [from, to], giữ thứ tự (ngày, giờ bắt đầu). */
int sched_expand(uint16_t user_id, int32_t from, int32_t to, occurrence_t *out, int max)
{
    (void) user_id;
    expand_calls++;
    if (expand_overflow) return -1;
    int k = 0;
    for (int i = 0; i < fake_n_occ && k < max; i++)
        if (fake_occ[i].day >= from && fake_occ[i].day <= to) out[k++] = fake_occ[i];
    return k;
}

bool sched_label_at(uint32_t source_id, uint8_t kind, int32_t orig_day,
                    char title[65], char room[17], char cls[17])
{
    (void) kind;
    (void) orig_day;
    snprintf(title, 65, "Nguon %u", (unsigned) source_id);
    snprintf(room, 17, "P%u", (unsigned) source_id);
    snprintf(cls, 17, "L%u", (unsigned) source_id);
    return true;
}

bool sched_label(uint32_t source_id, uint8_t kind, char title[65], char room[17], char cls[17])
{
    return sched_label_at(source_id, kind, 0, title, room, cls);
}

bool sched_room_override(uint32_t source_id, int32_t orig_day, char room[17])
{
    (void) source_id;
    (void) orig_day;
    (void) room;
    return false;
}

int32_t time_today(void)
{
    return fake_today;
}

static bool ack_full;

/* Ghi vào bảng g_acks của conflict.c (calendar_is_acked dùng chung). */
bool calendar_ack_add(uint32_t a, uint32_t b, int32_t day)
{
    if (a > b) {
        uint32_t t = a;
        a = b;
        b = t;
    }
    for (int i = 0; i < g_n_acks; i++)
        if (g_acks[i].source_a == a && g_acks[i].source_b == b && g_acks[i].day == day) return true;
    if (ack_full || g_n_acks >= CAL_MAX_ACKS) return false;
    g_acks[g_n_acks++] = (cal_ack_t) {.source_a = a, .source_b = b, .day = day};
    return true;
}

/* ---------- Fixture ---------- */

static void reset_fixture(void)
{
    command_count = 0;
    push_count = 0;
    last_push_user = 0;
    last_push_entity[0] = '\0';
    fake_n_occ = 0;
    expand_calls = 0;
    expand_overflow = false;
    fake_today = -1;
    ack_full = false;
    g_n_acks = 0;
    ld_lock_init(&g_cal_lock);
    calendar_register_cmds();
}

static int find_command(const char *name)
{
    for (int i = 0; i < command_count; i++)
        if (strcmp(commands[i].name, name) == 0) return i;
    return -1;
}

static int invoke(const char *name, uint16_t user_id, const char *json, cJSON *out)
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

static const cJSON *item(const cJSON *obj, const char *key)
{
    return cJSON_GetObjectItemCaseSensitive(obj, key);
}

static int array_size(const cJSON *obj, const char *key)
{
    const cJSON *arr = item(obj, key);
    return cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : -1;
}

static int32_t day_of(const char *text)
{
    int32_t d = 0;
    return dt_parse_date(text, &d) ? d : -1;
}

static void add_occ(const char *date, const char *start, const char *end,
                    uint32_t source_id, uint16_t campus_id, uint8_t status)
{
    occurrence_t *o = &fake_occ[fake_n_occ++];
    memset(o, 0, sizeof(*o));
    o->source_id = source_id;
    o->kind = OCC_TEACH;
    o->status = status;
    o->day = day_of(date);
    o->orig_day = o->day;
    dt_parse_hm(start, &o->start_min);
    dt_parse_hm(end, &o->end_min);
    o->campus_id = campus_id;
    o->school_id = 1;
}

/* ---------- Tests ---------- */

TEST(commands_registered_for_students)
{
    reset_fixture();
    static const char *names[] = {
        "calendar.week", "calendar.range", "conflict.list", "conflict.check", "conflict.ack",
    };
    CHECK_EQ(command_count, 5);
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        int c = find_command(names[i]);
        CHECK(c >= 0);
        if (c >= 0) CHECK_EQ(commands[c].role, LD_ROLE_STUDENT);
    }
}

TEST(calendar_week_returns_items_and_range)
{
    reset_fixture();
    add_occ("2026-11-02", "08:00", "09:00", 1, 1, OCC_NORMAL);   // thứ Hai
    add_occ("2026-11-04", "10:00", "11:00", 2, 1, OCC_NORMAL);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("calendar.week", 7, "{\"date\":\"2026-11-04\"}", out), LD_OK);
    CHECK(strcmp(item(out, "from")->valuestring, "2026-11-02") == 0);
    CHECK(strcmp(item(out, "to")->valuestring, "2026-11-08") == 0);
    CHECK_EQ(array_size(out, "items"), 2);
    CHECK_EQ(array_size(out, "conflicts"), 0);
    CHECK(item(out, "truncated") == NULL);

    const cJSON *first = cJSON_GetArrayItem(item(out, "items"), 0);
    CHECK_EQ(item(first, "sourceId")->valueint, 1);
    CHECK(strcmp(item(first, "date")->valuestring, "2026-11-02") == 0);
    CHECK(strcmp(item(first, "start")->valuestring, "08:00") == 0);
    CHECK(strcmp(item(first, "title")->valuestring, "Nguon 1") == 0);
    CHECK(strcmp(item(first, "school")->valuestring, "A") == 0);
    cJSON_Delete(out);
}

TEST(calendar_week_uses_esp_date_or_rejects_without_time)
{
    reset_fixture();
    fake_today = day_of("2026-11-04");
    add_occ("2026-11-05", "07:00", "08:00", 3, 2, OCC_NORMAL);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("calendar.week", 7, "{}", out), LD_OK);
    CHECK(strcmp(item(out, "from")->valuestring, "2026-11-02") == 0);
    CHECK_EQ(array_size(out, "items"), 1);
    cJSON_Delete(out);

    fake_today = -1;                                  // ESP chưa đồng bộ giờ
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("calendar.week", 7, "{}", out), LD_ERR_BAD_ARG);
    CHECK(strcmp(item(out, "_field")->valuestring, "date") == 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("calendar.week", 7, "{\"date\":\"2026-13-01\"}", out), LD_ERR_BAD_ARG);
    cJSON_Delete(out);
}

TEST(calendar_range_validates_length)
{
    reset_fixture();
    add_occ("2026-11-02", "08:00", "09:00", 1, 1, OCC_NORMAL);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("calendar.range", 7, "{\"from\":\"2026-11-02\",\"to\":\"2026-11-08\"}", out), LD_OK);
    CHECK(strcmp(item(out, "from")->valuestring, "2026-11-02") == 0);
    CHECK(strcmp(item(out, "to")->valuestring, "2026-11-08") == 0);
    CHECK_EQ(array_size(out, "items"), 1);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("calendar.range", 7, "{\"from\":\"2026-11-08\",\"to\":\"2026-11-02\"}", out),
             LD_ERR_BAD_ARG);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("calendar.range", 7, "{\"from\":\"2026-01-01\",\"to\":\"2026-03-04\"}", out),
             LD_ERR_BAD_ARG);                          // 63 ngày
    CHECK(strcmp(item(out, "_field")->valuestring, "to") == 0);
    cJSON_Delete(out);

    expand_overflow = true;
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("calendar.range", 7, "{\"from\":\"2026-11-02\",\"to\":\"2026-11-08\"}", out),
             LD_ERR_BAD_ARG);                          // sched_expand trả -1
    cJSON_Delete(out);
}

TEST(calendar_week_marks_conflicts_unless_acked)
{
    reset_fixture();
    add_occ("2026-11-02", "08:00", "09:30", 1, 1, OCC_NORMAL);
    add_occ("2026-11-02", "09:00", "10:00", 2, 1, OCC_NORMAL);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("calendar.week", 7, "{\"date\":\"2026-11-02\"}", out), LD_OK);
    CHECK_EQ(array_size(out, "conflicts"), 1);
    const cJSON *cf = cJSON_GetArrayItem(item(out, "conflicts"), 0);
    CHECK_EQ(item(cf, "a")->valueint, 1);
    CHECK_EQ(item(cf, "b")->valueint, 2);
    CHECK(cJSON_IsFalse(item(cf, "acked")));
    CHECK_EQ(item(cf, "reason")->valueint, CONFLICT_OVERLAP);
    cJSON_Delete(out);

    CHECK(calendar_ack_add(1, 2, day_of("2026-11-02")));

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("calendar.week", 7, "{\"date\":\"2026-11-02\"}", out), LD_OK);
    CHECK_EQ(array_size(out, "conflicts"), 1);
    CHECK(cJSON_IsTrue(item(cJSON_GetArrayItem(item(out, "conflicts"), 0), "acked")));
    const cJSON *first = cJSON_GetArrayItem(item(out, "items"), 0);
    CHECK(item(first, "conflicts") == NULL);          // đã xác nhận thì không đánh dấu trên buổi
    cJSON_Delete(out);
}

TEST(conflict_list_reports_pair_and_ack)
{
    reset_fixture();
    add_occ("2026-11-02", "08:00", "09:30", 1, 1, OCC_NORMAL);
    add_occ("2026-11-02", "09:00", "10:00", 2, 1, OCC_NORMAL);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.list", 7,
                    "{\"from\":\"2026-11-02\",\"to\":\"2026-11-02\"}", out), LD_OK);
    CHECK_EQ(array_size(out, "conflicts"), 1);
    const cJSON *row = cJSON_GetArrayItem(item(out, "conflicts"), 0);
    CHECK(cJSON_IsObject(item(row, "a")));
    CHECK(cJSON_IsObject(item(row, "b")));
    CHECK_EQ(item(item(row, "a"), "sourceId")->valueint, 1);
    CHECK(strcmp(item(row, "start")->valuestring, "09:00") == 0);
    CHECK(strcmp(item(row, "end")->valuestring, "09:30") == 0);
    CHECK(cJSON_IsFalse(item(row, "acked")));
    cJSON_Delete(out);

    CHECK(calendar_ack_add(1, 2, day_of("2026-11-02")));
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.list", 7,
                    "{\"from\":\"2026-11-02\",\"to\":\"2026-11-02\"}", out), LD_OK);
    CHECK(cJSON_IsTrue(item(cJSON_GetArrayItem(item(out, "conflicts"), 0), "acked")));
    cJSON_Delete(out);
}

TEST(conflict_list_skips_cancelled_and_needs_time)
{
    reset_fixture();
    add_occ("2026-11-02", "08:00", "09:00", 1, 1, OCC_NORMAL);
    add_occ("2026-11-02", "08:30", "09:30", 2, 1, OCC_CANCELLED);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.list", 7,
                    "{\"from\":\"2026-11-02\",\"to\":\"2026-11-02\"}", out), LD_OK);
    CHECK_EQ(array_size(out, "conflicts"), 0);        // buổi đã huỷ không tính
    cJSON_Delete(out);

    fake_today = -1;
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.list", 7, "{}", out), LD_ERR_BAD_ARG);
    cJSON_Delete(out);

    fake_today = day_of("2026-11-02");
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.list", 7, "{}", out), LD_OK);   // mặc định tuần hiện tại
    CHECK(strcmp(item(out, "from")->valuestring, "2026-11-02") == 0);
    CHECK(strcmp(item(out, "to")->valuestring, "2026-11-08") == 0);
    cJSON_Delete(out);
}

TEST(conflict_check_new_occurrence)
{
    reset_fixture();
    add_occ("2026-11-02", "08:00", "09:00", 1, 1, OCC_NORMAL);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.check", 7,
                    "{\"date\":\"2026-11-02\",\"start\":\"08:30\",\"end\":\"09:30\"}", out),
             LD_OK);                                  // luôn OK, kết quả nằm trong data
    CHECK_EQ(array_size(out, "conflicts"), 1);
    const cJSON *row = cJSON_GetArrayItem(item(out, "conflicts"), 0);
    CHECK_EQ(item(item(row, "with"), "sourceId")->valueint, 1);
    CHECK(strcmp(item(row, "start")->valuestring, "08:30") == 0);
    CHECK(strcmp(item(row, "end")->valuestring, "09:00") == 0);
    CHECK_EQ(item(row, "reason")->valueint, CONFLICT_OVERLAP);
    cJSON_Delete(out);

    // Buổi mới không trùng buổi nào.
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.check", 7,
                    "{\"date\":\"2026-11-02\",\"start\":\"10:00\",\"end\":\"11:00\"}", out),
             LD_OK);
    CHECK_EQ(array_size(out, "conflicts"), 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.check", 7,
                    "{\"date\":\"2026-11-02\",\"start\":\"11:00\",\"end\":\"11:00\"}", out),
             LD_ERR_BAD_ARG);
    CHECK(strcmp(item(out, "_field")->valuestring, "end") == 0);
    cJSON_Delete(out);
}

TEST(conflict_check_existing_occurrence_ignores_itself)
{
    reset_fixture();
    add_occ("2026-11-02", "08:00", "09:00", 1, 1, OCC_NORMAL);

    // Dời buổi nguồn 1 sang 10:00 cùng ngày: không tự báo trùng với chính nó.
    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.check", 7,
                    "{\"sourceId\":1,\"origDate\":\"2026-11-02\","
                    "\"date\":\"2026-11-02\",\"start\":\"10:00\",\"end\":\"11:00\"}",
                    out),
             LD_OK);
    CHECK_EQ(array_size(out, "conflicts"), 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.check", 7,
                    "{\"sourceId\":99,\"origDate\":\"2026-11-02\","
                    "\"date\":\"2026-11-02\",\"start\":\"10:00\",\"end\":\"11:00\"}",
                    out),
             LD_ERR_NOT_FOUND);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.check", 7,
                    "{\"sourceId\":1,\"date\":\"2026-11-02\","
                    "\"start\":\"10:00\",\"end\":\"11:00\"}",
                    out),
             LD_ERR_BAD_ARG);                         // thiếu origDate
    cJSON_Delete(out);
}

TEST(conflict_ack_requires_real_pair_and_notifies_owner)
{
    reset_fixture();
    add_occ("2026-11-02", "08:00", "09:30", 1, 1, OCC_NORMAL);
    add_occ("2026-11-02", "09:00", "10:00", 2, 1, OCC_NORMAL);

    cJSON *out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.ack", 7,
                    "{\"a\":1,\"b\":2,\"date\":\"2026-11-02\"}", out), LD_OK);
    CHECK_EQ(g_n_acks, 1);
    CHECK_EQ(g_acks[0].source_a, (uint32_t) 1);
    CHECK_EQ(g_acks[0].source_b, (uint32_t) 2);
    CHECK_EQ(push_count, 1);
    CHECK_EQ(last_push_user, (uint16_t) 7);
    CHECK(strcmp(last_push_entity, "conflict") == 0);
    cJSON_Delete(out);

    out = cJSON_CreateObject();                        // lặp lại không thêm bản ghi
    CHECK_EQ(invoke("conflict.ack", 7,
                    "{\"a\":2,\"b\":1,\"date\":\"2026-11-02\"}", out), LD_OK);
    CHECK_EQ(g_n_acks, 1);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.ack", 7,
                    "{\"a\":1,\"b\":3,\"date\":\"2026-11-02\"}", out), LD_ERR_NOT_FOUND);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.ack", 7,
                    "{\"a\":1,\"b\":2,\"date\":\"2026-11-03\"}", out), LD_ERR_NOT_FOUND);
    cJSON_Delete(out);

    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.ack", 7,
                    "{\"a\":1,\"date\":\"2026-11-02\"}", out), LD_ERR_BAD_ARG);
    cJSON_Delete(out);

    ack_full = true;
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.ack", 7,
                    "{\"a\":2,\"b\":1,\"date\":\"2026-11-02\"}", out), LD_OK); // đã có, không cần chỗ mới
    cJSON_Delete(out);

    g_n_acks = 0;                                      // bảng đầy thật sự
    ack_full = false;
    for (int i = 0; i < CAL_MAX_ACKS; i++)
        g_acks[i] = (cal_ack_t) {.source_a = (uint32_t) 100 + (uint32_t) i,
                                 .source_b = (uint32_t) 1000 + (uint32_t) i,
                                 .day = day_of("2026-11-02")};
    g_n_acks = CAL_MAX_ACKS;
    out = cJSON_CreateObject();
    CHECK_EQ(invoke("conflict.ack", 7,
                    "{\"a\":1,\"b\":2,\"date\":\"2026-11-02\"}", out), LD_ERR_FULL);
    cJSON_Delete(out);
}

int main(void)
{
    RUN(commands_registered_for_students);
    RUN(calendar_week_returns_items_and_range);
    RUN(calendar_week_uses_esp_date_or_rejects_without_time);
    RUN(calendar_range_validates_length);
    RUN(calendar_week_marks_conflicts_unless_acked);
    RUN(conflict_list_reports_pair_and_ack);
    RUN(conflict_list_skips_cancelled_and_needs_time);
    RUN(conflict_check_new_occurrence);
    RUN(conflict_check_existing_occurrence_ignores_itself);
    RUN(conflict_ack_requires_real_pair_and_notifies_owner);
    return REPORT();
}
