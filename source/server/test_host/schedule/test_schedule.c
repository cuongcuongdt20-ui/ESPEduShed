// Test của TV3: sinh buổi từ lịch lặp. Chạy: make schedule (dùng catalog giả, không phụ thuộc TV2)
#include "test.h"
#include "dt.h"
#include "schedule.h"
#include "schedule_internal.h"

TEST(lap_hang_tuan_thang_9)
{
    schedule_core_init();
    schedule_seed_sample();            // 2 lịch thứ Tư của user 1, 01/09/2026–30/11/2026
    occurrence_t occ[64];
    int n = sched_expand(1, dt_from_ymd(2026, 9, 1), dt_from_ymd(2026, 9, 30), occ, 64);
    CHECK_EQ(n, 10);                   // 5 thứ Tư (2, 9, 16, 23, 30) × 2 lịch
    CHECK_EQ(occ[0].start_min, 7 * 60 + 10);
    CHECK_EQ(dt_weekday(occ[0].day), 3);
}

TEST(nguoi_khac_khong_thay_lich)
{
    schedule_core_init();
    schedule_seed_sample();
    occurrence_t occ[64];
    CHECK_EQ(sched_expand(2, dt_from_ymd(2026, 9, 1), dt_from_ymd(2026, 9, 30), occ, 64), 0);
}

TEST(khoang_qua_dai_bi_tu_choi)
{
    occurrence_t occ[4];
    CHECK_EQ(sched_expand(1, dt_from_ymd(2026, 9, 1), dt_from_ymd(2026, 12, 31), occ, 4), -1);
}

TEST(vuot_max_bi_tu_choi)
{
    schedule_core_init();
    schedule_seed_sample();
    occurrence_t occ[3];
    CHECK_EQ(sched_expand(1, dt_from_ymd(2026, 9, 1), dt_from_ymd(2026, 9, 30), occ, 3), -1);
}

// TODO(TV3): ngoại lệ huỷ / dời / dạy bù, ngày nghỉ, lịch một lần, công việc lặp, kết quả được sắp theo ngày giờ

int main(void)
{
    RUN(lap_hang_tuan_thang_9);
    RUN(nguoi_khac_khong_thay_lich);
    RUN(khoang_qua_dai_bi_tu_choi);
    RUN(vuot_max_bi_tu_choi);
    return REPORT();
}
