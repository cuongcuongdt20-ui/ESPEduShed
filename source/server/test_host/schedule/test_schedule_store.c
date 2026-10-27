// Test host cho lưu trữ TV3 (schedule_store.c) theo định dạng JSON Lines.
// Dùng schedule_store.c thật; thay LittleFS (store_*), store_save_later và esp_log bằng fake.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "dt.h"
#include "platform.h"
#include "schedule.h"
#include "schedule_internal.h"
#include "test.h"

/* ---------- Fake file / store writer / reader ---------- */

static char  *g_file_text;
static bool   g_has_file;
static int    g_commit_calls;
static int    g_save_later_calls;
static bool   g_fail_begin;

struct store_writer {
    char  *buf;
    size_t len, cap;
    bool   ok;
};

static bool append(char **buf, size_t *len, size_t *cap, const char *s, size_t n)
{
    if (*len + n + 1 > *cap) {
        size_t next = (*cap ? *cap * 2 : 256);
        while (next < *len + n + 1) next *= 2;
        char *p = realloc(*buf, next);
        if (!p) return false;
        *buf = p;
        *cap = next;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = 0;
    return true;
}

store_writer_t *store_begin(const char *path)
{
    (void) path;
    if (g_fail_begin) return NULL;
    store_writer_t *w = calloc(1, sizeof(*w));
    if (!w) return NULL;
    w->ok = true;
    return w;
}

bool store_put_line(store_writer_t *w, const char *line)
{
    if (!w || !w->ok) return false;
    w->ok = append(&w->buf, &w->len, &w->cap, line, strlen(line))
            && append(&w->buf, &w->len, &w->cap, "\n", 1);
    return w->ok;
}

esp_err_t store_end(store_writer_t *w, bool commit)
{
    if (!w) return ESP_FAIL;
    bool ok = commit && w->ok;
    if (ok) {
        free(g_file_text);
        g_file_text = w->buf;
        w->buf = NULL;
        g_has_file = true;
        g_commit_calls++;
    }
    free(w->buf);
    free(w);
    return ok ? ESP_OK : (commit ? ESP_FAIL : ESP_OK);
}

struct store_reader {
    char  *data;
    size_t len, pos;
};

store_reader_t *store_open_lines(const char *path)
{
    (void) path;
    if (!g_has_file || !g_file_text) return NULL;
    store_reader_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->len = strlen(g_file_text);
    r->data = malloc(r->len + 1);
    if (!r->data) {
        free(r);
        return NULL;
    }
    memcpy(r->data, g_file_text, r->len + 1);
    return r;
}

int store_next_line(store_reader_t *r, char *buf, size_t cap)
{
    if (!r || r->pos >= r->len) return 0;
    size_t start = r->pos;
    while (r->pos < r->len && r->data[r->pos] != '\n') r->pos++;
    size_t n = r->pos - start;
    if (r->pos < r->len) r->pos++;               // bỏ '\n'
    if (n >= cap) return -1;                     // dòng quá dài: bỏ qua
    memcpy(buf, r->data + start, n);
    while (n > 0 && buf[n - 1] == '\r') n--;
    buf[n] = 0;
    return n == 0 ? store_next_line(r, buf, cap) : (int) n;
}

void store_close_lines(store_reader_t *r)
{
    if (!r) return;
    free(r->data);
    free(r);
}

char *store_read_all(const char *path, size_t *len)
{
    (void) path;
    if (!g_has_file || !g_file_text) return NULL;
    size_t n = strlen(g_file_text);
    char *copy = malloc(n + 1);
    if (!copy) return NULL;
    memcpy(copy, g_file_text, n + 1);
    if (len) *len = n;
    return copy;
}

esp_err_t store_write_atomic(const char *path, const char *data, size_t len)
{
    (void) path;
    char *copy = malloc(len + 1);
    if (!copy) return ESP_FAIL;
    memcpy(copy, data, len);
    copy[len] = 0;
    free(g_file_text);
    g_file_text = copy;
    g_has_file = true;
    return ESP_OK;
}

/* Timer ghi gộp: test chạy ngay để kiểm tra nội dung file. */
void store_save_later(store_save_fn fn)
{
    g_save_later_calls++;
    if (fn) fn();
}

const char *esp_err_to_name(esp_err_t code)
{
    (void) code;
    return "FAKE_ERR";
}

/* schedule_init() chỉ khai báo khi build firmware. */
void schedule_init(void);
void schedule_register_cmds(void) { }

/* ---------- Fixture ---------- */

static void reset_fixture(void)
{
    free(g_file_text);
    g_file_text = NULL;
    g_has_file = false;
    g_commit_calls = 0;
    g_save_later_calls = 0;
    g_fail_begin = false;
    schedule_core_init();
}

static void set_file(const char *text)
{
    free(g_file_text);
    g_file_text = strdup(text);
    g_has_file = g_file_text != NULL;
}

static int count_kind(const char *kind)
{
    int n = 0;
    char line[2048];
    char needle[32];
    snprintf(needle, sizeof(needle), "\"k\":\"%s\"", kind);
    store_reader_t *r = store_open_lines("/data/schedule.json");
    if (!r) return -1;
    while (store_next_line(r, line, sizeof(line)) > 0)
        if (strstr(line, needle)) n++;
    store_close_lines(r);
    return n;
}

TEST(first_boot_seed_can_be_saved)
{
    reset_fixture();
    schedule_init();                              // chưa có file → seed mẫu

    CHECK_EQ(g_sched.n_series, 2);
    CHECK(!g_has_file);
    CHECK_EQ(g_save_later_calls, 0);

    schedule_mark_dirty();                        // lần lưu đầu tiên phải tạo file
    CHECK_EQ(g_save_later_calls, 1);
    CHECK_EQ(g_commit_calls, 1);
    CHECK(g_has_file);

    CHECK(strstr(g_file_text, "{\"schema\":2,") != NULL);   // dòng đầu JSON Lines
    CHECK_EQ(count_kind("series"), 2);
}

TEST(save_then_load_roundtrip)
{
    reset_fixture();
    schedule_init();
    CHECK_EQ(g_sched.n_series, 2);

    g_sched.n_tasks = 1;
    g_sched.tasks[0] = (sched_task_t) {
        .id = 20,
        .owner_id = 1,
        .type = 6,
        .title = "Nop bao cao",
        .content = "Ban cuoi",
        .day = dt_from_ymd(2026, 11, 2),
        .start_min = 23 * 60 + 59,
        .end_min = 23 * 60 + 59,
        .until = dt_from_ymd(2026, 11, 16),
        .place = "Online",
        .status = 1,
        .note = "gap",
    };
    g_sched.n_exceptions = 1;
    g_sched.exceptions[0] = (sched_exception_t) {
        .series_id = 1,
        .orig_day = dt_from_ymd(2026, 9, 2),
        .type = EXC_ROOM,
        .new_room = "B-204",
        .note = "doi phong",
    };

    schedule_mark_dirty();
    CHECK_EQ(count_kind("task"), 1);
    CHECK_EQ(count_kind("exception"), 1);

    schedule_core_init();
    CHECK_EQ(g_sched.n_series, 0);
    CHECK_EQ(schedule_load(), true);

    CHECK_EQ(g_sched.n_series, 2);
    CHECK_EQ(g_sched.next_id, 10);
    CHECK_EQ(g_sched.n_tasks, 1);
    CHECK_EQ(g_sched.n_exceptions, 1);
    CHECK_EQ(g_sched.tasks[0].id, 20);
    CHECK(strcmp(g_sched.tasks[0].title, "Nop bao cao") == 0);
    CHECK_EQ(g_sched.tasks[0].until, dt_from_ymd(2026, 11, 16));
    CHECK_EQ(g_sched.tasks[0].start_min, g_sched.tasks[0].end_min);
    CHECK(strcmp(g_sched.exceptions[0].new_room, "B-204") == 0);
    CHECK_EQ(g_sched.exceptions[0].type, EXC_ROOM);
}

TEST(invalid_file_is_not_overwritten)
{
    reset_fixture();
    set_file("day khong phai json\n");
    char *before = strdup(g_file_text);

    CHECK_EQ(schedule_load(), false);
    CHECK_EQ(g_sched.n_series, 0);

    schedule_mark_dirty();
    CHECK_EQ(g_commit_calls, 0);
    CHECK(strcmp(g_file_text, before) == 0);

    free(before);
}

TEST(legacy_schema1_is_loaded_and_migrated_on_save)
{
    reset_fixture();
    set_file("{\"schema\":1,\"nextId\":10,\"series\":["
             "{\"id\":1,\"ownerId\":1,\"classId\":0,"
             "\"subjectCode\":\"CS101\",\"subjectName\":\"Lap trinh\","
             "\"classCode\":\"N01\",\"room\":\"A2\",\"note\":\"\","
             "\"credits\":3,\"schoolId\":1,\"campusId\":1,\"size\":40,"
             "\"weekday\":3,\"timeMode\":1,\"firstPeriod\":0,\"periodCount\":0,"
             "\"start\":\"08:00\",\"end\":\"09:00\","
             "\"from\":\"2026-09-01\",\"to\":\"2026-09-30\",\"weekly\":true}],"
             "\"tasks\":[],\"exceptions\":[]}");

    CHECK_EQ(schedule_load(), true);
    CHECK_EQ(g_sched.n_series, 1);
    CHECK_EQ(g_sched.next_id, 10);

    schedule_mark_dirty();                        // ghi lại thành JSON Lines
    CHECK(strstr(g_file_text, "{\"schema\":2,") != NULL);
    CHECK_EQ(count_kind("series"), 1);
}

TEST(load_rejects_duplicate_or_orphan_records)
{
    reset_fixture();
    set_file("{\"schema\":2,\"nextId\":5}\n"
             "{\"k\":\"series\",\"id\":1,\"ownerId\":1,\"classId\":0,"
             "\"subjectCode\":\"A\",\"subjectName\":\"A\",\"classCode\":\"\","
             "\"room\":\"\",\"note\":\"\",\"credits\":0,\"schoolId\":1,"
             "\"campusId\":0,\"size\":0,\"weekday\":1,\"timeMode\":1,"
             "\"firstPeriod\":0,\"periodCount\":0,\"start\":\"08:00\","
             "\"end\":\"09:00\",\"from\":\"2026-09-01\",\"to\":\"2026-09-02\","
             "\"weekly\":true}\n"
             "{\"k\":\"series\",\"id\":1,\"ownerId\":1,\"classId\":0,"
             "\"subjectCode\":\"B\",\"subjectName\":\"B\",\"classCode\":\"\","
             "\"room\":\"\",\"note\":\"\",\"credits\":0,\"schoolId\":1,"
             "\"campusId\":0,\"size\":0,\"weekday\":1,\"timeMode\":1,"
             "\"firstPeriod\":0,\"periodCount\":0,\"start\":\"08:00\","
             "\"end\":\"09:00\",\"from\":\"2026-09-01\",\"to\":\"2026-09-02\","
             "\"weekly\":true}\n");
    CHECK_EQ(schedule_load(), false);             // trùng id

    set_file("{\"schema\":2,\"nextId\":5}\n"
             "{\"k\":\"exception\",\"seriesId\":9,\"origDay\":\"2026-09-02\","
             "\"type\":1,\"newDay\":null,\"newStart\":\"00:00\","
             "\"newEnd\":\"00:00\",\"newRoom\":\"\",\"note\":\"\"}\n");
    CHECK_EQ(schedule_load(), false);             // ngoại lệ không có lịch
}

TEST(write_failure_keeps_ram_and_can_retry)
{
    reset_fixture();
    schedule_init();
    g_fail_begin = true;

    schedule_mark_dirty();                        // không mở được file tạm
    CHECK_EQ(g_commit_calls, 0);
    CHECK(!g_has_file);
    CHECK_EQ(g_sched.n_series, 2);

    g_fail_begin = false;
    schedule_mark_dirty();
    CHECK(g_has_file);
    CHECK_EQ(g_commit_calls, 1);
}

int main(void)
{
    RUN(first_boot_seed_can_be_saved);
    RUN(save_then_load_roundtrip);
    RUN(invalid_file_is_not_overwritten);
    RUN(legacy_schema1_is_loaded_and_migrated_on_save);
    RUN(load_rejects_duplicate_or_orphan_records);
    RUN(write_failure_keeps_ram_and_can_retry);
    return REPORT();
}
