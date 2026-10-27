// Test của TV3: sinh buổi từ lịch lặp. Chạy: make schedule (dùng catalog giả, không phụ thuộc TV2)
#include "test.h"
#include "dt.h"
#include "schedule.h"
#include "schedule_internal.h"
#include <string.h>

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


static void setup_one_series(void)
{
    schedule_core_init();

    g_sched.n_series = 1;
    g_sched.series[0] = (sched_series_t) {
        .id = 10,
        .owner_id = 1,
        .school_id = 0,
        .campus_id = 2,
        .weekday = 3,
        .time_mode = TIME_DIRECT,
        .start_min = 9 * 60,
        .end_min = 10 * 60,
        .from = 0,
        .to = 30000,
        .weekly = true
    };
}

TEST(huỷ_buổi_vẫn_trả_trạng_thái_cancelled)
{
    setup_one_series();

    g_sched.n_exceptions = 1;
    g_sched.exceptions[0] = (sched_exception_t) {
        .series_id = 10,
        .orig_day = dt_from_ymd(2026, 9, 2),
        .type = EXC_CANCEL
    };

    occurrence_t out[8];
    int n = sched_expand(
        1,
        dt_from_ymd(2026, 9, 2),
        dt_from_ymd(2026, 9, 2),
        out, 8
    );

    CHECK_EQ(n, 1);
    CHECK_EQ(out[0].status, OCC_CANCELLED);
}

TEST(doi_gio_buoi_hoc)
{
    setup_one_series();

    g_sched.n_exceptions = 1;
    g_sched.exceptions[0] = (sched_exception_t) {
        .series_id = 10,
        .orig_day = dt_from_ymd(2026, 9, 2),
        .type = EXC_TIME,
        .new_start = 13 * 60,
        .new_end = 14 * 60
    };

    occurrence_t out[8];
    int n = sched_expand(
        1,
        dt_from_ymd(2026, 9, 2),
        dt_from_ymd(2026, 9, 2),
        out, 8
    );

    CHECK_EQ(n, 1);
    CHECK_EQ(out[0].status, OCC_CHANGED);
    CHECK_EQ(out[0].start_min, 13 * 60);
    CHECK_EQ(out[0].end_min, 14 * 60);
}

TEST(doi_ngay_buoi_hoc)
{
    setup_one_series();

    int32_t original = dt_from_ymd(2026, 9, 2);
    int32_t moved = dt_from_ymd(2026, 9, 3);

    g_sched.n_exceptions = 1;
    g_sched.exceptions[0] = (sched_exception_t) {
        .series_id = 10,
        .orig_day = original,
        .type = EXC_MOVE,
        .new_day = moved,
        .new_start = 13 * 60,
        .new_end = 14 * 60
    };

    occurrence_t out[8];
    int n = sched_expand(1, original, moved, out, 8);

    CHECK_EQ(n, 1);
    CHECK_EQ(out[0].day, moved);
    CHECK_EQ(out[0].orig_day, original);
    CHECK_EQ(out[0].status, OCC_CHANGED);
    CHECK_EQ(out[0].start_min, 13 * 60);
}

TEST(tao_buoi_hoc_bu)
{
    setup_one_series();

    int32_t day = dt_from_ymd(2026, 9, 2);

    g_sched.n_exceptions = 1;
    g_sched.exceptions[0] = (sched_exception_t) {
        .series_id = 10,
        .orig_day = day,
        .type = EXC_MAKEUP,
        .new_day = day,
        .new_start = 13 * 60,
        .new_end = 14 * 60
    };

    occurrence_t out[8];
    int n = sched_expand(1, day, day, out, 8);

    CHECK_EQ(n, 2);

    int found_makeup = 0;
    for (int i = 0; i < n; ++i) {
        if (out[i].kind == OCC_MAKEUP) {
            found_makeup++;
            CHECK_EQ(out[i].status, OCC_CHANGED);
            CHECK_EQ(out[i].start_min, 13 * 60);
        }
    }

    CHECK_EQ(found_makeup, 1);
}

TEST(cong_viec_lap_hang_tuan)
{
    schedule_core_init();

    g_sched.n_tasks = 1;
    g_sched.tasks[0] = (sched_task_t) {
        .id = 20,
        .owner_id = 1,
        .day = dt_from_ymd(2026, 9, 1),
        .until = dt_from_ymd(2026, 9, 15),
        .start_min = 10 * 60,
        .end_min = 10 * 60 + 30,
        .campus_id = 2,
        .status = 0
    };

    occurrence_t out[8];
    int n = sched_expand(
        1,
        dt_from_ymd(2026, 9, 1),
        dt_from_ymd(2026, 9, 15),
        out, 8
    );

    CHECK_EQ(n, 3);
    CHECK_EQ(out[0].kind, OCC_TASK);
    CHECK_EQ(out[0].day, dt_from_ymd(2026, 9, 1));
    CHECK_EQ(out[1].day, dt_from_ymd(2026, 9, 8));
    CHECK_EQ(out[2].day, dt_from_ymd(2026, 9, 15));
}

TEST(day_bu_huy_buoi_goc_va_them_buoi_bu)
{
    setup_one_series();

    int32_t orig = dt_from_ymd(2026, 9, 2);       // thứ Tư
    int32_t makeup = dt_from_ymd(2026, 9, 5);     // dạy bù thứ Bảy

    g_sched.n_exceptions = 1;
    g_sched.exceptions[0] = (sched_exception_t) {
        .series_id = 10, .orig_day = orig, .type = EXC_MAKEUP,
        .new_day = makeup, .new_start = 8 * 60, .new_end = 9 * 60
    };

    occurrence_t out[8];
    int n = sched_expand(1, orig, makeup, out, 8);

    CHECK_EQ(n, 2);
    CHECK_EQ(out[0].day, orig);                   // sắp theo ngày: buổi gốc trước
    CHECK_EQ(out[0].status, OCC_CANCELLED);
    CHECK_EQ(out[1].day, makeup);
    CHECK_EQ(out[1].kind, OCC_MAKEUP);
    CHECK_EQ(out[1].orig_day, orig);
}

TEST(han_chot_hien_tren_lich)
{
    schedule_core_init();

    g_sched.n_tasks = 1;
    g_sched.tasks[0] = (sched_task_t) {
        .id = 30, .owner_id = 1, .day = dt_from_ymd(2026, 10, 30),
        .start_min = 23 * 60 + 59, .end_min = 23 * 60 + 59        // hạn chót 23:59, không có khoảng giờ
    };

    occurrence_t out[4];
    int n = sched_expand(1, dt_from_ymd(2026, 10, 26), dt_from_ymd(2026, 11, 1), out, 4);

    CHECK_EQ(n, 1);
    CHECK_EQ(out[0].kind, OCC_TASK);
    CHECK_EQ(out[0].start_min, out[0].end_min);
}


TEST(nhan_phong_ap_dung_ngoai_le_doi_phong)
{
    setup_one_series();
    strcpy(g_sched.series[0].room, "A2-301");

    int32_t changed_day = dt_from_ymd(2026, 9, 2);
    g_sched.n_exceptions = 1;
    g_sched.exceptions[0] = (sched_exception_t) {
        .series_id = 10,
        .orig_day = changed_day,
        .type = EXC_ROOM,
        .new_room = "B-204"
    };

    char room[17];
    CHECK(sched_room_override(10, changed_day, room));
    CHECK(strcmp(room, "B-204") == 0);            // buổi bị đổi phòng
    CHECK(!sched_room_override(10, dt_from_ymd(2026, 9, 9), room));   // buổi khác phòng gốc
}

TEST(khoang_rong_va_bien_ngay)
{
    setup_one_series();                            // thứ Tư, 09:00-10:00

    occurrence_t out[8];
    CHECK_EQ(sched_expand(1, dt_from_ymd(2026, 9, 3), dt_from_ymd(2026, 9, 3), out, 8), 0);
    CHECK_EQ(sched_expand(1, dt_from_ymd(2026, 8, 31), dt_from_ymd(2026, 9, 1), out, 8), 0);
    CHECK_EQ(sched_expand(1, dt_from_ymd(2026, 9, 2), dt_from_ymd(2026, 9, 2), out, 8), 1);
    CHECK_EQ(out[0].day, dt_from_ymd(2026, 9, 2));
}

TEST(cong_viec_lap_khop_vao_khoang_truy_van)
{
    schedule_core_init();
    g_sched.n_tasks = 1;
    g_sched.tasks[0] = (sched_task_t) {
        .id = 20, .owner_id = 1,
        .day = dt_from_ymd(2026, 9, 1),
        .until = dt_from_ymd(2026, 9, 15),
        .start_min = 10 * 60, .end_min = 10 * 60 + 30
    };

    occurrence_t out[8];
    int n = sched_expand(1, dt_from_ymd(2026, 9, 3), dt_from_ymd(2026, 9, 10), out, 8);
    CHECK_EQ(n, 1);
    CHECK_EQ(out[0].day, dt_from_ymd(2026, 9, 8));   // mốc lặp vẫn tính từ ngày bắt đầu
}

int main(void)
{
    RUN(lap_hang_tuan_thang_9);
    RUN(nguoi_khac_khong_thay_lich);
    RUN(khoang_qua_dai_bi_tu_choi);
    RUN(vuot_max_bi_tu_choi);
    RUN(huỷ_buổi_vẫn_trả_trạng_thái_cancelled);
    RUN(doi_gio_buoi_hoc);
    RUN(doi_ngay_buoi_hoc);
    RUN(tao_buoi_hoc_bu);
    RUN(cong_viec_lap_hang_tuan);
    RUN(day_bu_huy_buoi_goc_va_them_buoi_bu);
    RUN(han_chot_hien_tren_lich);
    RUN(nhan_phong_ap_dung_ngoai_le_doi_phong);
    RUN(khoang_rong_va_bien_ngay);
    RUN(cong_viec_lap_khop_vao_khoang_truy_van);
    return REPORT();
}
