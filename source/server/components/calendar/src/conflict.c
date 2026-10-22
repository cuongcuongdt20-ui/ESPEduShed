// Kiểm tra trùng lịch (TV4). Logic thuần, test được trên máy tính.
//
// BẢN GIẢ: chưa phát hiện xung đột nào.
// TODO(TV4): thuật toán quét (sweep) trên danh sách đã sắp theo (ngày, giờ bắt đầu):
//   - trùng giờ: a.start < b.end && b.start < a.end  → CONFLICT_OVERLAP, overlap = [max start, min end]
//   - khác cơ sở và khoảng nghỉ giữa hai buổi < catalog_travel_minutes(a.campus, b.campus) → CONFLICT_TRAVEL
//   - bỏ qua buổi OCC_CANCELLED và OCC_HOLIDAY
//   Nêu độ phức tạp trong báo cáo.
#include <stdlib.h>
#include "conflict.h"
#include "calendar_internal.h"
#include "catalog.h"

cal_ack_t g_acks[CAL_MAX_ACKS];
int       g_n_acks;
ld_lock_t g_cal_lock;

int conflict_find_in(const occurrence_t *occ, int n, conflict_t *out, int max)
{
    (void) occ;
    (void) n;
    (void) out;
    (void) max;
    return 0;
}

int conflict_check(uint16_t user_id, const occurrence_t *candidates, int n, uint32_t ignore_source_id,
                   conflict_t *out, int max)
{
    // TODO(TV4): với mỗi khoảng ngày của ứng viên (chia đoạn tối đa SCHED_MAX_RANGE_DAYS):
    //   sched_expand(user_id, ...) → bỏ buổi có source_id == ignore_source_id → gộp với ứng viên → sắp xếp
    //   → conflict_find_in. Chỉ giữ xung đột có ít nhất một bên là ứng viên.
    (void) user_id;
    (void) candidates;
    (void) n;
    (void) ignore_source_id;
    (void) out;
    (void) max;
    return 0;
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
