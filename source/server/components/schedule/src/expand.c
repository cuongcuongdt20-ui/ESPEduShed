
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "schedule.h"
#include "schedule_internal.h"
#include "catalog.h"
#include "dt.h"

typedef struct {
    sched_series_t *series;
    int n_series;
    sched_task_t *tasks;
    int n_tasks;
    sched_exception_t *exceptions;
    int n_exceptions;
} schedule_snapshot_t;

static void snapshot_free(schedule_snapshot_t *snap)
{
    if (!snap) return;

    free(snap->series);
    free(snap->tasks);
    free(snap->exceptions);
    memset(snap, 0, sizeof(*snap));
}

/*
 * Read counts, allocate only the needed space, then copy under the lock.
 * If the schedule grows between the two lock acquisitions, fail safely.
 */
static bool snapshot_load(schedule_snapshot_t *snap)
{
    memset(snap, 0, sizeof(*snap));

    ld_lock(g_sched_lock);
    uint16_t ns = g_sched.n_series;
    uint16_t nt = g_sched.n_tasks;
    uint16_t ne = g_sched.n_exceptions;
    ld_unlock(g_sched_lock);

    if (ns > SCHED_MAX_SERIES ||
        nt > SCHED_MAX_TASKS ||
        ne > SCHED_MAX_EXCEPTIONS) {
        return false;
    }

    snap->series = ns ? malloc((size_t)ns * sizeof(*snap->series)) : NULL;
    snap->tasks = nt ? malloc((size_t)nt * sizeof(*snap->tasks)) : NULL;
    snap->exceptions =
        ne ? malloc((size_t)ne * sizeof(*snap->exceptions)) : NULL;

    if ((ns && !snap->series) ||
        (nt && !snap->tasks) ||
        (ne && !snap->exceptions)) {
        snapshot_free(snap);
        return false;
    }

    ld_lock(g_sched_lock);

    if (g_sched.n_series > ns ||
        g_sched.n_tasks > nt ||
        g_sched.n_exceptions > ne ||
        g_sched.n_series > SCHED_MAX_SERIES ||
        g_sched.n_tasks > SCHED_MAX_TASKS ||
        g_sched.n_exceptions > SCHED_MAX_EXCEPTIONS) {
        ld_unlock(g_sched_lock);
        snapshot_free(snap);
        return false;
    }

    snap->n_series = g_sched.n_series;
    snap->n_tasks = g_sched.n_tasks;
    snap->n_exceptions = g_sched.n_exceptions;

    if (snap->n_series) {
        memcpy(snap->series, g_sched.series,
               (size_t)snap->n_series * sizeof(*snap->series));
    }
    if (snap->n_tasks) {
        memcpy(snap->tasks, g_sched.tasks,
               (size_t)snap->n_tasks * sizeof(*snap->tasks));
    }
    if (snap->n_exceptions) {
        memcpy(snap->exceptions, g_sched.exceptions,
               (size_t)snap->n_exceptions * sizeof(*snap->exceptions));
    }

    ld_unlock(g_sched_lock);
    return true;
}

static bool series_occurs_on(const sched_series_t *s, int32_t day)
{
    if (day < s->from || day > s->to) return false;

    if (!s->weekly) return day == s->from;

    return s->weekday >= 1 &&
           s->weekday <= 7 &&
           dt_weekday(day) == s->weekday;
}

static const sched_exception_t *find_exception(
    const schedule_snapshot_t *snap,
    uint32_t series_id,
    int32_t orig_day)
{
    for (int i = 0; i < snap->n_exceptions; ++i) {
        const sched_exception_t *e = &snap->exceptions[i];

        if (e->series_id == series_id && e->orig_day == orig_day) {
            return e;
        }
    }

    return NULL;
}

static bool series_time(
    const sched_series_t *s,
    uint16_t *start,
    uint16_t *end)
{
    *start = s->start_min;
    *end = s->end_min;

    if (s->time_mode == TIME_BY_PERIOD) {
        if (!catalog_period_to_time(
                s->school_id,
                s->first_period,
                s->period_count,
                start,
                end)) {
            return false;
        }
    } else if (s->time_mode != TIME_DIRECT) {
        return false;
    }

    return *start < *end && *end <= 1440;
}

static void apply_holiday(occurrence_t *occ)
{
    char holiday_name[65] = {0};

    /*
     * School ID 0 is used for tasks or entries without a school.
     * Do not query the school holiday catalog for those entries.
     */
    if (occ->school_id != 0 &&
        occ->status != OCC_CANCELLED &&
        catalog_is_holiday(occ->day, occ->school_id, holiday_name)) {
        occ->status = OCC_HOLIDAY;
    }
}

static bool append_occurrence(
    occurrence_t *out,
    int max,
    int *count,
    occurrence_t occ)
{
    if (*count >= max) return false;

    apply_holiday(&occ);
    out[(*count)++] = occ;
    return true;
}

static bool valid_exception_time(uint16_t start, uint16_t end)
{
    return start < end && end <= 1440;
}

static int compare_occurrences(const void *lhs, const void *rhs)
{
    const occurrence_t *a = lhs;
    const occurrence_t *b = rhs;

    if (a->day != b->day) return a->day < b->day ? -1 : 1;
    if (a->start_min != b->start_min)
        return a->start_min < b->start_min ? -1 : 1;
    if (a->end_min != b->end_min)
        return a->end_min < b->end_min ? -1 : 1;
    if (a->source_id != b->source_id)
        return a->source_id < b->source_id ? -1 : 1;
    if (a->kind != b->kind)
        return a->kind < b->kind ? -1 : 1;

    return 0;
}

int sched_expand(
    uint16_t user_id,
    int32_t from,
    int32_t to,
    occurrence_t *out,
    int max)
{
    if (to < from ||
        (int64_t)to - (int64_t)from + 1 > SCHED_MAX_RANGE_DAYS ||
        out == NULL ||
        max <= 0) {
        return -1;
    }

    schedule_snapshot_t snap;
    if (!snapshot_load(&snap)) return -1;

    int n = 0;
    bool overflow = false;

    /*
     * 1. Expand teaching series.
     */
    for (int i = 0; i < snap.n_series && !overflow; ++i) {
        const sched_series_t *s = &snap.series[i];

        if (s->owner_id != user_id) continue;

        uint16_t base_start, base_end;
        if (!series_time(s, &base_start, &base_end)) continue;

        int64_t first = from > s->from ? from : s->from;
        int64_t last = to < s->to ? to : s->to;

        for (int64_t d = first; d <= last; ++d) {
            int32_t day = (int32_t)d;
            if (!series_occurs_on(s, day)) continue;

            const sched_exception_t *e =
                find_exception(&snap, s->id, day);

            /*
             * A moved occurrence is emitted at new_day below,
             * not at its original day.
             */
            if (e && e->type == EXC_MOVE) continue;

            occurrence_t occ = {
                .source_id = s->id,
                .kind = OCC_TEACH,
                .status = OCC_NORMAL,
                .day = day,
                .orig_day = day,
                .start_min = base_start,
                .end_min = base_end,
                .school_id = s->school_id,
                .campus_id = s->campus_id
            };

            if (e) {
                switch (e->type) {
                case EXC_CANCEL:
                    occ.status = OCC_CANCELLED;
                    break;

                case EXC_ROOM:
                    /*
                     * occurrence_t has no room field.
                     * Keep the status so consumers can recognize
                     * a changed occurrence; room details belong
                     * to the schedule detail/label API.
                     */
                    occ.status = OCC_CHANGED;
                    break;

                case EXC_TIME:
                    if (!valid_exception_time(e->new_start, e->new_end)) {
                        continue;
                    }
                    occ.start_min = e->new_start;
                    occ.end_min = e->new_end;
                    occ.status = OCC_CHANGED;
                    break;

                case EXC_MOVE:
                    /* Handled above. */
                    break;

                case EXC_MAKEUP:
                    /*
                     * Dạy bù thay cho buổi gốc: buổi gốc thành đã huỷ,
                     * buổi bù được thêm ở bước 2 (ngày new_day).
                     */
                    occ.status = OCC_CANCELLED;
                    break;

                default:
                    /* Ignore unknown exception types safely. */
                    break;
                }
            }

            if (!append_occurrence(out, max, &n, occ)) {
                overflow = true;
                break;
            }
        }
    }

    /*
     * 2. Emit moved and makeup occurrences.
     *
     * Process exceptions independently of the original date range,
     * so a moved occurrence can appear even when orig_day is outside
     * the requested range.
     */
    for (int i = 0; i < snap.n_exceptions && !overflow; ++i) {
        const sched_exception_t *e = &snap.exceptions[i];

        if (e->type != EXC_MOVE && e->type != EXC_MAKEUP) continue;
        if (e->new_day < from || e->new_day > to) continue;

        const sched_series_t *s = NULL;
        for (int j = 0; j < snap.n_series; ++j) {
            if (snap.series[j].id == e->series_id &&
                snap.series[j].owner_id == user_id) {
                s = &snap.series[j];
                break;
            }
        }

        if (!s || !series_occurs_on(s, e->orig_day)) continue;

        uint16_t start, end;
        if (!series_time(s, &start, &end)) continue;

        if (valid_exception_time(e->new_start, e->new_end)) {
            start = e->new_start;
            end = e->new_end;
        }

        occurrence_t occ = {
            .source_id = s->id,
            .kind = e->type == EXC_MAKEUP ? OCC_MAKEUP : OCC_TEACH,
            .status = OCC_CHANGED,
            .day = e->new_day,
            .orig_day = e->orig_day,
            .start_min = start,
            .end_min = end,
            .school_id = s->school_id,
            .campus_id = s->campus_id
        };

        if (!append_occurrence(out, max, &n, occ)) {
            overflow = true;
            break;
        }
    }

    /*
     * 3. Expand tasks. A task with until == 0 occurs once.
     * Otherwise it repeats every seven days through until.
     */
    for (int i = 0; i < snap.n_tasks && !overflow; ++i) {
        const sched_task_t *t = &snap.tasks[i];

        if (t->owner_id != user_id) continue;
        // start == end là hạn chót (không có khoảng giờ): vẫn hiện, TV4 không tính trùng
        if (t->start_min > t->end_min || t->end_min > 1440) continue;

        int64_t last = t->until == 0 ? t->day : t->until;
        if (last < t->day) continue;

        int64_t first_day = t->day > from ? t->day : from;
        int64_t last_day = last < to ? last : to;

        if (t->until == 0) {
            if (t->day < from || t->day > to) continue;
            first_day = last_day = t->day;
        } else {
            /*
             * Align the first in-range date with the recurrence
             * anchored at t->day, including ranges before the start.
             */
            if (first_day < t->day) first_day = t->day;

            int64_t delta = first_day - (int64_t)t->day;
            first_day += ((7 - (delta % 7)) % 7);
        }

        for (int64_t d = first_day; d <= last_day; d += 7) {
            occurrence_t occ = {
                .source_id = t->id,
                .kind = OCC_TASK,
                .status = t->status == 3 ? OCC_CANCELLED : OCC_NORMAL,
                .day = (int32_t)d,
                .orig_day = (int32_t)d,
                .start_min = t->start_min,
                .end_min = t->end_min,
                .school_id = 0,
                .campus_id = t->campus_id
            };

            if (!append_occurrence(out, max, &n, occ)) {
                overflow = true;
                break;
            }
        }
    }

    snapshot_free(&snap);

    if (overflow) return -1;

    qsort(out, (size_t)n, sizeof(*out), compare_occurrences);
    return n;
}
