// Thư viện ngày giờ (TV2). Thuật toán days_from_civil / civil_from_days của Howard Hinnant (lịch Gregory).
#include <stdio.h>
#include "dt.h"

int32_t dt_from_ymd(int y, int m, int d)
{
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void dt_to_ymd(int32_t day, int *y, int *m, int *d)
{
    int32_t z = day + 719468;
    int era = (z >= 0 ? z : z - 146096) / 146097;
    int doe = z - era * 146097;
    int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = yoe + era * 400 + (*m <= 2);
}

int dt_weekday(int32_t day)
{
    // 1970-01-01 là thứ Năm (4)
    int w = (int) ((day % 7 + 7) % 7);   // 0 = thứ Năm
    return (w + 3) % 7 + 1;
}

int32_t dt_week_start(int32_t day)
{
    return day - (dt_weekday(day) - 1);
}

static bool leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

bool dt_parse_date(const char *s, int32_t *out)
{
    int y, m, d;
    char tail;
    if (!s || sscanf(s, "%4d-%2d-%2d%c", &y, &m, &d, &tail) != 3) return false;
    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (y < 2000 || y > 2099 || m < 1 || m > 12 || d < 1) return false;
    int max = mdays[m - 1] + (m == 2 && leap(y));
    if (d > max) return false;
    *out = dt_from_ymd(y, m, d);
    return true;
}

void dt_format_date(int32_t day, char out[11])
{
    int y, m, d;
    dt_to_ymd(day, &y, &m, &d);
    snprintf(out, 11, "%04d-%02d-%02d", y, m, d);
}

bool dt_parse_hm(const char *s, uint16_t *out)
{
    int h, m;
    char tail;
    if (!s || sscanf(s, "%2d:%2d%c", &h, &m, &tail) != 2) return false;
    if (h < 0 || h > 23 || m < 0 || m > 59) return false;
    *out = (uint16_t) (h * 60 + m);
    return true;
}

void dt_format_hm(uint16_t min, char out[6])
{
    snprintf(out, 6, "%02u:%02u", (unsigned) (min / 60) % 24, (unsigned) min % 60);
}
