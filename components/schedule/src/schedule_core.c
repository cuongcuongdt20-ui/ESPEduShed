// Logic thuần của lịch (TV3): mô hình, nhãn hiển thị, kiểm tra tham chiếu. Test được trên máy tính.
#include <string.h>
#include "schedule.h"
#include "schedule_internal.h"
#include "dt.h"

schedule_t g_sched;
ld_lock_t  g_sched_lock;

void schedule_core_init(void)
{
    ld_lock_init(&g_sched_lock);
    memset(&g_sched, 0, sizeof(g_sched));
    g_sched.next_id = 1;
}

void schedule_seed_sample(void)
{
    // Lập trình mạng, thứ Tư, trường A tiết 3–4, 01/09/2026–30/11/2026 (ví dụ trong đề bài)
    sched_series_t *s = &g_sched.series[g_sched.n_series++];
    memset(s, 0, sizeof(*s));
    s->id = 1;
    strcpy(s->subject_code, "INT1433");
    strcpy(s->subject_name, "Lap trinh mang");
    strcpy(s->class_code, "N01");
    s->credits = 3;
    s->school_id = 1;
    s->campus_id = 1;
    strcpy(s->room, "A2-301");
    s->weekday = 3;
    s->time_mode = TIME_BY_PERIOD;
    s->first_period = 3;
    s->period_count = 2;
    s->from = dt_from_ymd(2026, 9, 1);
    s->to = dt_from_ymd(2026, 11, 30);
    s->weekly = true;

    // Trường B, thứ Tư 08:50–09:40 (tiết 3): trùng giờ với lịch trên để thử cảnh báo
    sched_series_t *t = &g_sched.series[g_sched.n_series++];
    *t = *s;
    t->id = 2;
    strcpy(t->subject_name, "Mang may tinh");
    strcpy(t->class_code, "L02");
    t->school_id = 2;
    t->campus_id = 2;
    strcpy(t->room, "B-105");
    t->first_period = 3;
    t->period_count = 1;
    g_sched.next_id = 10;
}

bool sched_label(uint32_t source_id, uint8_t kind, char title[65], char room[17], char class_code[17])
{
    bool found = false;
    ld_lock(g_sched_lock);
    if (kind == OCC_TEACH || kind == OCC_MAKEUP) {
        for (int i = 0; i < g_sched.n_series && !found; i++) {
            const sched_series_t *s = &g_sched.series[i];
            if (s->id != source_id) continue;
            strcpy(title, s->subject_name);
            strcpy(room, s->room);
            strcpy(class_code, s->class_code);
            found = true;
        }
    } else if (kind == OCC_TASK) {
        for (int i = 0; i < g_sched.n_tasks && !found; i++) {
            const sched_task_t *t = &g_sched.tasks[i];
            if (t->id != source_id) continue;
            strcpy(title, t->title);
            memcpy(room, t->place, 16);     // nhãn ngắn: lấy 16 byte đầu của địa điểm
            room[16] = 0;
            class_code[0] = 0;
            found = true;
        }
    }
    // TODO(TV3): buổi có ngoại lệ đổi phòng thì nhãn phòng lấy từ ngoại lệ (cần thêm orig_day nếu muốn chính xác)
    ld_unlock(g_sched_lock);
    return found;
}

bool sched_uses_school(uint16_t school_id)
{
    bool used = false;
    ld_lock(g_sched_lock);
    for (int i = 0; i < g_sched.n_series && !used; i++) used = g_sched.series[i].school_id == school_id;
    ld_unlock(g_sched_lock);
    return used;
}

bool sched_uses_class(uint16_t class_id)
{
    bool used = false;
    ld_lock(g_sched_lock);
    for (int i = 0; i < g_sched.n_series && !used; i++) used = g_sched.series[i].class_id == class_id;
    ld_unlock(g_sched_lock);
    return used;
}
