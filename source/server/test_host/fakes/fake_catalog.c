// Bản giả của catalog.h cho test của TV3 và TV4: bảng tiết cố định, không phụ thuộc code TV2.
#include <string.h>
#include "catalog.h"

bool catalog_school_exists(uint16_t school_id)
{
    return school_id == 1 || school_id == 2;
}

bool catalog_period_to_time(uint16_t school_id, uint8_t first, uint8_t count, uint16_t *start, uint16_t *end)
{
    if (school_id == 1 && first == 3 && count == 2) {   // trường A tiết 3–4
        *start = 7 * 60 + 10;
        *end = 9 * 60 + 15;
        return true;
    }
    if (school_id == 2 && first == 3 && count == 1) {   // trường B tiết 3
        *start = 8 * 60 + 50;
        *end = 9 * 60 + 40;
        return true;
    }
    return false;
}

bool catalog_is_holiday(int32_t day, uint16_t school_id, char name_out[65])
{
    (void) day;
    (void) school_id;
    (void) name_out;
    return false;
}

uint16_t catalog_travel_minutes(uint16_t a, uint16_t b)
{
    return a == b || a == 0 || b == 0 ? 0 : 30;
}

bool catalog_school_label(uint16_t school_id, char short_name[17], char color[8])
{
    strcpy(short_name, school_id == 1 ? "A" : "B");
    strcpy(color, "#000000");
    return true;
}
