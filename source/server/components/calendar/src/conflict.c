// Kiểm tra trùng lịch (TV4). Logic thuần, test được trên máy tính.
//
// Thuật toán quét (sweep) trên danh sách đã sắp theo (ngày, giờ bắt đầu):
//   - trùng giờ: a.start < b.end && b.start < a.end  -> CONFLICT_OVERLAP, overlap = [max start, min end]
//   - khác cơ sở và khoảng nghỉ giữa hai buổi < catalog_travel_minutes(a.campus, b.campus) -> CONFLICT_TRAVEL
//   - bỏ qua buổi OCC_CANCELLED, OCC_HOLIDAY và buổi không có khoảng giờ (hạn chót: start >= end)
// Với mỗi buổi i chỉ xét các buổi j > i cùng ngày: những buổi bắt đầu trước khi i kết thúc (trùng giờ), rồi đúng một
// buổi kế tiếp sau i (thời gian di chuyển chỉ tính giữa hai buổi liền nhau); danh sách đã sắp theo start nên dừng ở đó.
// Độ phức tạp O(n + k) (k = số cặp trùng giờ), xấu nhất O(n^2) khi mọi buổi chồng nhau.
// conflict_check: nạp lịch một lần cho cả cửa sổ ngày (tối đa 62 ngày); mỗi ứng viên so với các buổi cùng ngày: O(m·d).
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
            if (!counts(&occ[j])) continue;
            if (occ[j].start_min < occ[i].end_min) {          // bắt đầu trước khi i kết thúc: trùng giờ
                if (conflict_pair(&occ[i], &occ[j], &out[k])) k++;
                continue;
            }
            // j là buổi đầu tiên sau khi i kết thúc: chỉ cặp liền nhau này mới xét thời gian di chuyển
            // (có buổi khác chen giữa thì người dạy đi từ buổi chen giữa, không đi thẳng từ i)
            if (conflict_pair(&occ[i], &occ[j], &out[k])) k++;
            break;
        }
    }
    return k;
}

#define WIN_MAX 240            // số buổi tối đa nạp một lần cho cả cửa sổ ngày

int conflict_check(uint16_t user_id, const occurrence_t *candidates, int n, uint32_t ignore_source_id,
                   conflict_t *out, int max)
{
    if (n <= 0 || max <= 0) return 0;
    occurrence_t *buf = malloc(sizeof(occurrence_t) * WIN_MAX);
    if (!buf) return 0;
    int k = 0, have = 0;
    int32_t win_from = 1, win_to = 0;                 // cửa sổ ngày đang nạp trong buf (rỗng lúc đầu)
    for (int i = 0; i < n && k < max; i++) {
        const occurrence_t *c = &candidates[i];
        if (!counts(c)) continue;
        if (c->day < win_from || c->day > win_to) {
            // Nạp một lần cho cả khoảng tới ứng viên xa nhất (tối đa 62 ngày) thay vì từng ngày
            int32_t to = c->day;
            for (int m = i + 1; m < n; m++)
                if (candidates[m].day > to && candidates[m].day < c->day + SCHED_MAX_RANGE_DAYS) to = candidates[m].day;
            have = sched_expand(user_id, c->day, to, buf, WIN_MAX);
            if (have < 0) {                           // quá nhiều buổi trong khoảng: nạp riêng ngày này
                to = c->day;
                have = sched_expand(user_id, c->day, c->day, buf, WIN_MAX);
                if (have < 0) have = 0;
            }
            win_from = c->day;
            win_to = to;
        }
        // Trùng giờ: so với mọi buổi cùng ngày. Di chuyển: chỉ với buổi liền trước và liền sau ứng viên.
        // Bỏ qua đúng buổi đang được sửa (cùng nguồn, cùng ngày gốc), không bỏ các buổi khác của cùng lịch.
        const occurrence_t *prev = NULL, *next = NULL;
        for (int j = 0; j < have && k < max; j++) {
            const occurrence_t *e = &buf[j];
            if (e->day != c->day || !counts(e)) continue;
            if (e->source_id == ignore_source_id && e->orig_day == c->orig_day) continue;
            if (e->start_min < c->end_min && c->start_min < e->end_min) {
                if (conflict_pair(c, e, &out[k])) k++;
            } else if (e->end_min <= c->start_min) {
                if (!prev || e->end_min > prev->end_min) prev = e;
            } else if (!next || e->start_min < next->start_min) {
                next = e;
            }
        }
        if (prev && k < max && conflict_pair(c, prev, &out[k])) k++;
        if (next && k < max && conflict_pair(c, next, &out[k])) k++;
    }
    free(buf);
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
