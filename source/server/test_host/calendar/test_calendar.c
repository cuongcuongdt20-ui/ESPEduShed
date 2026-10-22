// Test của TV4: kiểm tra trùng lịch. Chạy: make calendar (dùng schedule và catalog giả)
// Các test dưới đây là YÊU CẦU: sẽ FAIL cho tới khi TV4 viết xong conflict_find_in / conflict_check.
#include "test.h"
#include "dt.h"
#include "conflict.h"
#include "calendar_internal.h"
#include "fake_schedule.h"

static occurrence_t occ(uint32_t src, int32_t day, int s, int e, uint16_t campus)
{
    return (occurrence_t) {.source_id = src, .kind = OCC_TEACH, .status = OCC_NORMAL, .day = day, .orig_day = day,
                           .start_min = (uint16_t) s, .end_min = (uint16_t) e, .school_id = 1, .campus_id = campus};
}

TEST(trung_gio_hai_truong)
{
    int32_t d = dt_from_ymd(2026, 9, 2);
    occurrence_t list[] = {occ(1, d, 8 * 60, 10 * 60, 1), occ(2, d, 9 * 60, 11 * 60, 2)};
    conflict_t c[4];
    int n = conflict_find_in(list, 2, c, 4);
    CHECK_EQ(n, 1);                                   // A 08:00–10:00 và B 09:00–11:00
    if (n == 1) {
        CHECK_EQ(c[0].reason, CONFLICT_OVERLAP);
        CHECK_EQ(c[0].overlap_start, 9 * 60);
        CHECK_EQ(c[0].overlap_end, 10 * 60);
    }
}

TEST(khong_kip_di_chuyen)
{
    int32_t d = dt_from_ymd(2026, 9, 2);
    occurrence_t list[] = {occ(1, d, 8 * 60, 10 * 60, 1), occ(2, d, 10 * 60 + 10, 11 * 60, 2)};   // nghỉ 10 phút < 30
    conflict_t c[4];
    CHECK_EQ(conflict_find_in(list, 2, c, 4), 1);
}

TEST(buoi_da_huy_khong_tinh)
{
    int32_t d = dt_from_ymd(2026, 9, 2);
    occurrence_t list[] = {occ(1, d, 8 * 60, 10 * 60, 1), occ(2, d, 9 * 60, 11 * 60, 1)};
    list[1].status = OCC_CANCELLED;
    conflict_t c[4];
    CHECK_EQ(conflict_find_in(list, 2, c, 4), 0);
}

TEST(kiem_tra_ung_vien)
{
    int32_t d = dt_from_ymd(2026, 9, 2);
    fake_occ[0] = occ(1, d, 8 * 60, 10 * 60, 1);
    fake_n_occ = 1;
    occurrence_t cand = occ(7, d, 9 * 60 + 30, 10 * 60 + 30, 1);
    conflict_t c[4];
    CHECK_EQ(conflict_check(1, &cand, 1, 7, c, 4), 1);
    CHECK_EQ(conflict_check(1, &cand, 1, 1, c, 4), 0);   // đang sửa chính lịch 1: bỏ qua
}

int main(void)
{
    ld_lock_init(&g_cal_lock);
    RUN(trung_gio_hai_truong);
    RUN(khong_kip_di_chuyen);
    RUN(buoi_da_huy_khong_tinh);
    RUN(kiem_tra_ung_vien);
    return REPORT();
}
