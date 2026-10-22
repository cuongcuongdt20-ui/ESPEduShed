// Test của TV2: thư viện ngày giờ và quy đổi tiết. Chạy: make catalog
#include <string.h>
#include "test.h"
#include "dt.h"
#include "catalog.h"
#include "catalog_internal.h"

TEST(ngay_va_thu)
{
    CHECK_EQ(dt_from_ymd(1970, 1, 1), 0);
    CHECK_EQ(dt_weekday(dt_from_ymd(2026, 9, 2)), 3);     // 02/09/2026 là thứ Tư
    CHECK_EQ(dt_weekday(dt_from_ymd(2026, 9, 6)), 7);     // Chủ nhật
    CHECK_EQ(dt_week_start(dt_from_ymd(2026, 9, 6)), dt_from_ymd(2026, 8, 31));
    int y, m, d;
    dt_to_ymd(dt_from_ymd(2028, 2, 29), &y, &m, &d);
    CHECK(y == 2028 && m == 2 && d == 29);
}

TEST(doc_ngay_gio)
{
    int32_t day;
    uint16_t min;
    CHECK(dt_parse_date("2028-02-29", &day));             // năm nhuận
    CHECK(!dt_parse_date("2026-02-29", &day));
    CHECK(!dt_parse_date("2026-13-01", &day));
    CHECK(!dt_parse_date("2026-09-01x", &day));
    CHECK(dt_parse_hm("07:10", &min) && min == 430);
    CHECK(!dt_parse_hm("24:00", &min));
    char s[11];
    dt_format_date(dt_from_ymd(2026, 12, 31), s);
    CHECK(strcmp(s, "2026-12-31") == 0);
}

TEST(quy_doi_tiet_hai_truong)
{
    catalog_core_init();
    catalog_seed_sample();
    uint16_t a, b;
    CHECK(catalog_period_to_time(1, 3, 2, &a, &b));      // trường A tiết 3–4 = 07:10–09:15
    CHECK_EQ(a, 7 * 60 + 10);
    CHECK_EQ(b, 9 * 60 + 15);
    CHECK(catalog_period_to_time(2, 13, 4, &a, &b));     // trường B tiết 13–16
    CHECK_EQ(a, 18 * 60);
    CHECK(!catalog_period_to_time(1, 7, 1, &a, &b));     // trường A không có tiết 7
    CHECK(!catalog_period_to_time(9, 1, 1, &a, &b));     // không có trường 9
    // TODO(TV2): khoảng tiết vắt qua chỗ trống (trường B tiết 3–13) phải bị từ chối
}

int main(void)
{
    RUN(ngay_va_thu);
    RUN(doc_ngay_gio);
    RUN(quy_doi_tiet_hai_truong);
    return REPORT();
}
