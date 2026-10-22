// Logic thuần của danh mục (TV2): không gọi API ESP-IDF, test được trên máy tính.
#include <string.h>
#include "catalog.h"
#include "catalog_internal.h"
#include "dt.h"

catalog_t g_cat;
ld_lock_t g_cat_lock;

void catalog_core_init(void)
{
    ld_lock_init(&g_cat_lock);
    memset(&g_cat, 0, sizeof(g_cat));
    g_cat.default_travel = 30;
    g_cat.next_id = 1;
}

static void add_period(cat_school_t *s, uint8_t no, const char *start, const char *end)
{
    cat_period_t *p = &s->periods[s->n_periods++];
    p->no = no;
    dt_parse_hm(start, &p->start);
    dt_parse_hm(end, &p->end);
}

void catalog_seed_sample(void)
{
    // Trường A (mẫu rút gọn): tiết 3–4 = 07:10–09:15, tiết 5–6 = 09:30–11:35 (theo đề bài)
    cat_school_t *a = &g_cat.schools[g_cat.n_schools++];
    a->id = 1;
    strcpy(a->name, "Truong A");
    strcpy(a->short_name, "A");
    strcpy(a->color, "#2E7D32");
    add_period(a, 3, "07:10", "08:10");
    add_period(a, 4, "08:15", "09:15");
    add_period(a, 5, "09:30", "10:30");
    add_period(a, 6, "10:35", "11:35");

    // Trường B: đánh số khác, có tiết 13–16 buổi tối
    cat_school_t *b = &g_cat.schools[g_cat.n_schools++];
    b->id = 2;
    strcpy(b->name, "Truong B");
    strcpy(b->short_name, "B");
    strcpy(b->color, "#1565C0");
    add_period(b, 1, "07:00", "07:50");
    add_period(b, 2, "07:55", "08:45");
    add_period(b, 3, "08:50", "09:40");
    add_period(b, 13, "18:00", "18:50");
    add_period(b, 14, "18:55", "19:45");
    add_period(b, 15, "19:50", "20:40");
    add_period(b, 16, "20:45", "21:35");

    g_cat.campuses[g_cat.n_campuses++] = (cat_campus_t) {.id = 1, .school_id = 1, .name = "Co so 1"};
    g_cat.campuses[g_cat.n_campuses++] = (cat_campus_t) {.id = 2, .school_id = 2, .name = "Co so chinh"};
    g_cat.next_id = 10;
}

const cat_school_t *cat_find_school(uint16_t id)
{
    for (int i = 0; i < g_cat.n_schools; i++)
        if (g_cat.schools[i].id == id) return &g_cat.schools[i];
    return NULL;
}

static const cat_period_t *find_period(const cat_school_t *s, uint8_t no)
{
    for (int i = 0; i < s->n_periods; i++)
        if (s->periods[i].no == no) return &s->periods[i];
    return NULL;
}

bool catalog_school_exists(uint16_t school_id)
{
    ld_lock(g_cat_lock);
    bool ok = cat_find_school(school_id) != NULL;
    ld_unlock(g_cat_lock);
    return ok;
}

bool catalog_period_to_time(uint16_t school_id, uint8_t first_period, uint8_t count,
                            uint16_t *start_min, uint16_t *end_min)
{
    if (count == 0) return false;
    bool ok = false;
    ld_lock(g_cat_lock);
    const cat_school_t *s = cat_find_school(school_id);
    if (s) {
        const cat_period_t *first = find_period(s, first_period);
        const cat_period_t *last = find_period(s, (uint8_t) (first_period + count - 1));
        // TODO(TV2): kiểm tra mọi tiết ở giữa đều tồn tại (khoảng tiết không được vắt qua chỗ trống)
        if (first && last && last->end > first->start) {
            *start_min = first->start;
            *end_min = last->end;
            ok = true;
        }
    }
    ld_unlock(g_cat_lock);
    return ok;
}

bool catalog_is_holiday(int32_t day, uint16_t school_id, char name_out[65])
{
    bool found = false;
    ld_lock(g_cat_lock);
    for (int i = 0; i < g_cat.n_holidays && !found; i++) {
        const cat_holiday_t *h = &g_cat.holidays[i];
        if ((h->school_id == 0 || h->school_id == school_id) && day >= h->from && day <= h->to) {
            if (name_out) strcpy(name_out, h->name);
            found = true;
        }
    }
    ld_unlock(g_cat_lock);
    return found;
}

uint16_t catalog_travel_minutes(uint16_t campus_a, uint16_t campus_b)
{
    if (campus_a == 0 || campus_b == 0 || campus_a == campus_b) return 0;
    ld_lock(g_cat_lock);
    uint16_t m = g_cat.default_travel;
    for (int i = 0; i < g_cat.n_travel; i++) {
        const cat_travel_t *t = &g_cat.travel[i];
        if ((t->campus_a == campus_a && t->campus_b == campus_b)
            || (t->campus_a == campus_b && t->campus_b == campus_a)) {
            m = t->minutes;
            break;
        }
    }
    ld_unlock(g_cat_lock);
    return m;
}

bool catalog_school_label(uint16_t school_id, char short_name[17], char color[8])
{
    ld_lock(g_cat_lock);
    const cat_school_t *s = cat_find_school(school_id);
    if (s) {
        strcpy(short_name, s->short_name);
        strcpy(color, s->color);
    }
    ld_unlock(g_cat_lock);
    return s != NULL;
}
