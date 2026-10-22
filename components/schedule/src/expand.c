// Sinh buổi từ quy tắc lặp (TV3). Logic thuần, test được trên máy tính.
//
// BẢN KHỞI ĐẦU: chỉ sinh lịch dạy lặp hàng tuần / một lần, quy đổi tiết qua catalog.
// TODO(TV3): áp dụng ngoại lệ (huỷ, đổi phòng, đổi giờ, dời ngày), thêm buổi dạy bù (OCC_MAKEUP),
//            đánh dấu OCC_HOLIDAY bằng catalog_is_holiday, sinh buổi công việc (OCC_TASK), sắp xếp kết quả.
#include <string.h>
#include "schedule.h"
#include "schedule_internal.h"
#include "catalog.h"
#include "dt.h"

// Bản sao những gì cần của một lịch, để không giữ khoá schedule khi gọi sang catalog
typedef struct {
    uint32_t id;
    uint16_t school_id, campus_id;
    uint8_t  weekday, time_mode, first_period, period_count;
    uint16_t start_min, end_min;
    int32_t  from, to;
    bool     weekly;
} series_view_t;

int sched_expand(int32_t from, int32_t to, occurrence_t *out, int max)
{
    if (to < from || to - from + 1 > SCHED_MAX_RANGE_DAYS) return -1;

    static series_view_t views[SCHED_MAX_SERIES];   // tĩnh để không tốn stack; chỉ một luồng gọi tại một lúc
    int nv = 0;
    ld_lock(g_sched_lock);
    for (int i = 0; i < g_sched.n_series; i++) {
        const sched_series_t *s = &g_sched.series[i];
        if (s->to < from || s->from > to) continue;
        views[nv++] = (series_view_t) {
            .id = s->id, .school_id = s->school_id, .campus_id = s->campus_id,
            .weekday = s->weekday, .time_mode = s->time_mode,
            .first_period = s->first_period, .period_count = s->period_count,
            .start_min = s->start_min, .end_min = s->end_min,
            .from = s->from, .to = s->to, .weekly = s->weekly,
        };
    }
    ld_unlock(g_sched_lock);
    // TODO(TV3): views tĩnh không an toàn khi hai task cùng gọi; dùng khoá riêng hoặc cấp phát động

    int n = 0;
    for (int i = 0; i < nv; i++) {
        const series_view_t *v = &views[i];
        uint16_t start = v->start_min, end = v->end_min;
        if (v->time_mode == TIME_BY_PERIOD
            && !catalog_period_to_time(v->school_id, v->first_period, v->period_count, &start, &end))
            continue;                                   // bảng tiết đã đổi, tiết không còn: bỏ qua buổi

        for (int32_t day = from; day <= to; day++) {
            bool hit = v->weekly ? (day >= v->from && day <= v->to && dt_weekday(day) == v->weekday)
                                 : day == v->from;
            if (!hit) continue;
            if (n >= max) return -1;
            out[n++] = (occurrence_t) {
                .source_id = v->id, .kind = OCC_TEACH, .status = OCC_NORMAL,
                .day = day, .orig_day = day, .start_min = start, .end_min = end,
                .school_id = v->school_id, .campus_id = v->campus_id,
            };
        }
    }
    return n;
}
