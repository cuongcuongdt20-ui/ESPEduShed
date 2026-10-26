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

TEST(sat_nhau_khong_trung)
{
    int32_t d = dt_from_ymd(2026, 9, 2);
    occurrence_t list[] = {occ(1, d, 8 * 60, 10 * 60, 1), occ(2, d, 10 * 60, 11 * 60, 1)};   // 10:00 nối 10:00, cùng cơ sở
    conflict_t c[4];
    CHECK_EQ(conflict_find_in(list, 2, c, 4), 0);
}

TEST(du_thoi_gian_di_chuyen)
{
    int32_t d = dt_from_ymd(2026, 9, 2);
    occurrence_t list[] = {occ(1, d, 8 * 60, 10 * 60, 1), occ(2, d, 10 * 60 + 30, 11 * 60, 2)};   // nghỉ đúng 30 phút
    conflict_t c[4];
    CHECK_EQ(conflict_find_in(list, 2, c, 4), 0);
    list[1].start_min = 10 * 60 + 29;
    CHECK_EQ(conflict_find_in(list, 2, c, 4), 1);
    CHECK_EQ(c[0].reason, CONFLICT_TRAVEL);
    CHECK_EQ(c[0].overlap_start, 10 * 60);          // khoảng thiếu: 10:00 → 10:29
    CHECK_EQ(c[0].overlap_end, 10 * 60 + 29);
    list[1].campus_id = 0;                          // học trực tuyến: không cần di chuyển
    CHECK_EQ(conflict_find_in(list, 2, c, 4), 0);
}

TEST(ba_khoi_chong_nhau)
{
    int32_t d = dt_from_ymd(2026, 9, 2);
    occurrence_t list[] = {occ(1, d, 8 * 60, 10 * 60, 1), occ(2, d, 8 * 60 + 30, 10 * 60, 1), occ(3, d, 9 * 60, 11 * 60, 1)};
    conflict_t c[8];
    CHECK_EQ(conflict_find_in(list, 3, c, 8), 3);   // 1-2, 1-3, 2-3
    CHECK_EQ(conflict_find_in(list, 3, c, 2), 2);   // dừng ở max
}

TEST(tuan_vat_thang_va_ngay_khac_nhau)
{
    int32_t d1 = dt_from_ymd(2026, 8, 31), d2 = dt_from_ymd(2026, 9, 1);   // thứ Hai 31/8 và thứ Ba 1/9
    occurrence_t list[] = {occ(1, d1, 8 * 60, 10 * 60, 1), occ(2, d2, 8 * 60, 10 * 60, 1), occ(3, d2, 9 * 60, 9 * 60 + 30, 1)};
    conflict_t c[4];
    CHECK_EQ(conflict_find_in(list, 3, c, 4), 1);   // chỉ 2-3 cùng ngày
    CHECK_EQ(c[0].a.source_id, 2);
    CHECK_EQ(c[0].b.source_id, 3);
}

TEST(nghi_le_va_han_chot_khong_tinh)
{
    int32_t d = dt_from_ymd(2026, 9, 2);
    occurrence_t list[] = {occ(1, d, 8 * 60, 10 * 60, 1), occ(2, d, 9 * 60, 9 * 60, 1), occ(3, d, 9 * 60, 11 * 60, 1)};
    list[2].status = OCC_HOLIDAY;
    conflict_t c[4];
    CHECK_EQ(conflict_find_in(list, 3, c, 4), 0);   // 2 là hạn chót (không có khoảng giờ), 3 là nghỉ lễ
}

TEST(ung_vien_nhieu_ngay)
{
    int32_t d = dt_from_ymd(2026, 9, 2);
    fake_occ[0] = occ(1, d, 8 * 60, 10 * 60, 1);
    fake_occ[1] = occ(2, d + 7, 8 * 60, 10 * 60, 1);
    fake_n_occ = 2;
    occurrence_t cand[] = {occ(9, d, 9 * 60, 10 * 60, 1), occ(9, d + 7, 10 * 60, 11 * 60, 1), occ(9, d + 14, 8 * 60, 9 * 60, 1)};
    conflict_t c[8];
    CHECK_EQ(conflict_check(1, cand, 3, 0, c, 8), 1);   // chỉ tuần đầu trùng
    CHECK_EQ(c[0].b.source_id + c[0].a.source_id, 10);
}

TEST(di_chuyen_chi_xet_hai_buoi_lien_nhau)
{
    int32_t d = dt_from_ymd(2026, 9, 2);
    // 07:00–08:00 cơ sở 1, 08:00–08:30 cơ sở 1, 08:50–09:30 cơ sở 2 (cần 30 phút): chỉ cặp thứ 2–3 thiếu thời gian
    occurrence_t list[] = {occ(1, d, 7 * 60, 8 * 60, 1), occ(2, d, 8 * 60, 8 * 60 + 30, 1), occ(3, d, 8 * 60 + 50, 9 * 60 + 30, 2)};
    conflict_t c[4];
    CHECK_EQ(conflict_find_in(list, 3, c, 4), 1);
    CHECK_EQ(c[0].a.source_id, 2);
    CHECK_EQ(c[0].reason, CONFLICT_TRAVEL);
}

TEST(doi_buoi_vao_gio_buoi_khac_cung_lich_van_bao)
{
    int32_t d = dt_from_ymd(2026, 9, 9);
    fake_occ[0] = occ(5, d, 8 * 60, 10 * 60, 1);          // buổi thường của lịch 5 ngày d
    fake_n_occ = 1;
    conflict_t c[4];
    occurrence_t moved = occ(5, d, 9 * 60, 10 * 60, 1);   // buổi tuần trước của lịch 5 dời sang ngày d
    moved.orig_day = d - 7;
    CHECK_EQ(conflict_check(1, &moved, 1, 5, c, 4), 1);
    occurrence_t same = occ(5, d, 9 * 60, 10 * 60, 1);    // chính buổi ngày d đổi giờ: không so với bản cũ của nó
    CHECK_EQ(conflict_check(1, &same, 1, 5, c, 4), 0);
}

TEST(ung_vien_chi_xet_di_chuyen_voi_buoi_lien_ke)
{
    int32_t d = dt_from_ymd(2026, 9, 16);
    fake_occ[0] = occ(1, d, 7 * 60, 8 * 60, 2);           // cơ sở 2, xa ứng viên (có buổi khác chen giữa)
    fake_occ[1] = occ(2, d, 8 * 60, 9 * 60, 1);           // cơ sở 1, liền trước ứng viên
    fake_n_occ = 2;
    occurrence_t cand = occ(9, d, 9 * 60 + 10, 10 * 60, 1);
    conflict_t c[4];
    CHECK_EQ(conflict_check(1, &cand, 1, 0, c, 4), 0);    // cùng cơ sở với buổi liền trước: không báo
}

int main(void)
{
    ld_lock_init(&g_cal_lock);
    RUN(trung_gio_hai_truong);
    RUN(khong_kip_di_chuyen);
    RUN(buoi_da_huy_khong_tinh);
    RUN(kiem_tra_ung_vien);
    RUN(sat_nhau_khong_trung);
    RUN(du_thoi_gian_di_chuyen);
    RUN(ba_khoi_chong_nhau);
    RUN(tuan_vat_thang_va_ngay_khac_nhau);
    RUN(nghi_le_va_han_chot_khong_tinh);
    RUN(ung_vien_nhieu_ngay);
    RUN(di_chuyen_chi_xet_hai_buoi_lien_nhau);
    RUN(doi_buoi_vao_gio_buoi_khac_cung_lich_van_bao);
    RUN(ung_vien_chi_xet_di_chuyen_voi_buoi_lien_ke);
    return REPORT();
}
