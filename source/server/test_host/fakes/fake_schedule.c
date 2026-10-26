// Bản giả của schedule.h cho test của TV4: test tự đặt danh sách buổi "đang có".
#include <string.h>
#include "schedule.h"
#include "fake_schedule.h"

occurrence_t fake_occ[64];
int          fake_n_occ;

int sched_expand(uint16_t user_id, int32_t from, int32_t to, occurrence_t *out, int max)
{
    (void) user_id;
    int n = 0;
    for (int i = 0; i < fake_n_occ; i++) {
        if (fake_occ[i].day < from || fake_occ[i].day > to) continue;
        if (n >= max) return -1;
        out[n++] = fake_occ[i];
    }
    return n;
}

bool sched_label(uint32_t source_id, uint8_t kind, char title[65], char room[17], char class_code[17])
{
    (void) kind;
    strcpy(title, source_id == 1 ? "Mon 1" : "Mon 2");
    room[0] = 0;
    class_code[0] = 0;
    return true;
}

bool sched_room_override(uint32_t source_id, int32_t orig_day, char room[17])
{
    (void) source_id;
    (void) orig_day;
    (void) room;
    return false;
}

bool sched_uses_school(uint16_t school_id)
{
    (void) school_id;
    return false;
}

bool sched_uses_class(uint16_t class_id)
{
    (void) class_id;
    return false;
}
