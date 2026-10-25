// Kiểm tra trùng lịch (TV4). Logic thuần, test được trên máy tính.
//
// Thuật toán quét (sweep) trên danh sách đã sắp theo (ngày, giờ bắt đầu):
//   - trùng giờ: a.start < b.end && b.start < a.end  -> CONFLICT_OVERLAP, overlap = [max start, min end]
//   - khác cơ sở và khoảng nghỉ giữa hai buổi < catalog_travel_minutes(a.campus, b.campus) -> CONFLICT_TRAVEL
//   - bỏ qua buổi OCC_CANCELLED, OCC_HOLIDAY và buổi không có khoảng giờ (hạn chót: start >= end)
// Độ phức tạp: với mỗi buổi i chỉ xét các buổi j > i cùng ngày có start_j < end_i + GAP_CAP, vì danh sách đã sắp
// theo start nên dừng ngay khi vượt. Trường hợp thường O(n + k) (k = số cặp được xét, nhỏ), xấu nhất O(n^2) khi mọi
// buổi chồng nhau. Phần ứng viên (conflict_check) thêm O(m log m) nếu phải sắp, ở đây không cần vì so từng cặp.
#include <stdlib.h>
#include "conflict.h"
#include "calendar_internal.h"
#include "catalog.h"

#define GAP_CAP 180            // thời gian di chuyển tối đa được xét (phút); lớn hơn thì coi như 180

cal_ack_t g_acks[CAL_MAX_ACKS];
int       g_n_acks;
ld_lock_t g_cal_lock;

static bool counts(const occurrence_t *o)
{
    return o->status != OCC_CANCELLED && o->status != OCC_HOLIDAY && o->start_min < o->end_min;
}

bool conflict_pair(const occurrence_t *x, const occurrence_t *y, conflict_t *cf)
{
    if (x->day != y->day || !counts(x) || !counts(y)) return false;
    const occurrence_t *a = x, *b = y;               // a bắt đầu trước
    if (b->start_min < a->start_min) {
        a = y;
        b = x;
    }
    uint16_t s = b->start_min > a->start_min ? b->start_min : a->start_min;
    uint16_t e = a->end_min < b->end_min ? a->end_min : b->end_min;
    uint8_t reason;
    uint16_t cs, ce;
    if (s < e) {
        reason = CONFLICT_OVERLAP;
        cs = s;
        ce = e;
    } else {
        // không chồng nhau: a kết thúc trước hoặc đúng lúc b bắt đầu
        if (a->campus_id == 0 || b->campus_id == 0 || a->campus_id == b->campus_id) return false;
        int need = catalog_travel_minutes(a->campus_id, b->campus_id);
        if (need > GAP_CAP) need = GAP_CAP;
        int gap = (int) b->start_min - (int) a->end_min;
        if (gap >= need) return false;
        reason = CONFLICT_TRAVEL;
        cs = a->end_min;
        ce = b->start_min;
    }
    if (cf) {
        cf->a = *a;
        cf->b = *b;
        cf->overlap_start = cs;
        cf->overlap_end = ce;
        cf->reason = reason;
    }
    return true;
}

int conflict_find_in(const occurrence_t *occ, int n, conflict_t *out, int max)
{
    int k = 0;
    for (int i = 0; i < n && k < max; i++) {
        if (!counts(&occ[i])) continue;
        for (int j = i + 1; j < n && k < max; j++) {
            if (occ[j].day != occ[i].day) break;
            if (occ[j].start_min >= occ[i].end_min + GAP_CAP) break;
            if (conflict_pair(&occ[i], &occ[j], &out[k])) k++;
        }
    }
    return k;
}

#define DAY_MAX 64             // số buổi tối đa của một người trong một ngày

int conflict_check(uint16_t user_id, const occurrence_t *candidates, int n, uint32_t ignore_source_id,
                   conflict_t *out, int max)
{
    if (n <= 0 || max <= 0) return 0;
    occurrence_t *day_occ = malloc(sizeof(occurrence_t) * DAY_MAX);
    if (!day_occ) return 0;
    int k = 0, have = 0;
    int32_t loaded = -1;                              // ngày đang nằm trong day_occ
    for (int i = 0; i < n && k < max; i++) {
        const occurrence_t *c = &candidates[i];
        if (!counts(c)) continue;
        if (c->day != loaded) {                       // ứng viên thường đã theo thứ tự ngày: mỗi ngày nạp một lần
            have = sched_expand(user_id, c->day, c->day, day_occ, DAY_MAX);
            if (have < 0) have = 0;
            loaded = c->day;
        }
        for (int j = 0; j < have && k < max; j++) {
            if (day_occ[j].source_id == ignore_source_id) continue;
            if (conflict_pair(c, &day_occ[j], &out[k])) k++;
        }
    }
    free(day_occ);
    return k;
}

bool calendar_is_acked(uint32_t a, uint32_t b, int32_t day)
{
    if (a > b) {
        uint32_t t = a;
        a = b;
        b = t;
    }
    bool found = false;
    ld_lock(g_cal_lock);
    for (int i = 0; i < g_n_acks && !found; i++)
        found = g_acks[i].source_a == a && g_acks[i].source_b == b && g_acks[i].day == day;
    ld_unlock(g_cal_lock);
    return found;
}
