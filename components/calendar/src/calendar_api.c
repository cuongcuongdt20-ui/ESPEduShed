// REST API lịch tuần, khoảng ngày, xung đột (TV4).
#include <stdlib.h>
#include "cJSON.h"
#include "platform.h"
#include "conflict.h"
#include "calendar_internal.h"
#include "catalog.h"
#include "dt.h"

#define MAX_OCC 400

static esp_err_t not_yet(httpd_req_t *req)
{
    return json_error(req, 501, "NOT_IMPLEMENTED", "Chua lam", NULL);
}

static cJSON *occ_json(const occurrence_t *o)
{
    char d[11], s[6], e[6], title[65] = "", room[17] = "", cls[17] = "", sn[17] = "", color[8] = "#888888";
    dt_format_date(o->day, d);
    dt_format_hm(o->start_min, s);
    dt_format_hm(o->end_min, e);
    sched_label(o->source_id, o->kind, title, room, cls);
    catalog_school_label(o->school_id, sn, color);

    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "sourceId", o->source_id);
    cJSON_AddNumberToObject(j, "kind", o->kind);
    cJSON_AddNumberToObject(j, "status", o->status);
    cJSON_AddStringToObject(j, "date", d);
    char od[11];
    dt_format_date(o->orig_day, od);
    cJSON_AddStringToObject(j, "origDate", od);
    cJSON_AddStringToObject(j, "start", s);
    cJSON_AddStringToObject(j, "end", e);
    cJSON_AddStringToObject(j, "title", title);
    cJSON_AddStringToObject(j, "room", room);
    cJSON_AddStringToObject(j, "classCode", cls);
    cJSON_AddNumberToObject(j, "school", o->school_id);
    cJSON_AddStringToObject(j, "schoolShort", sn);
    cJSON_AddStringToObject(j, "color", color);
    cJSON_AddItemToObject(j, "conflicts", cJSON_CreateArray());   // TODO(TV4): mã các xung đột của buổi này
    return j;
}

// Trả {"from":..,"to":..,"items":[...],"conflicts":[...]} cho khoảng [from, to]
static esp_err_t reply_range(httpd_req_t *req, int32_t from, int32_t to)
{
    occurrence_t *occ = malloc(sizeof(occurrence_t) * MAX_OCC);
    if (!occ) return json_error(req, 500, "INTERNAL", "Het bo nho", NULL);
    int n = sched_expand(from, to, occ, MAX_OCC);
    if (n < 0) {
        free(occ);
        return json_error(req, 400, "BAD_ARG", "Khoang ngay qua dai hoac qua nhieu buoi", NULL);
    }
    // TODO(TV4): conflict_find_in(occ, n, ...) rồi gắn mã xung đột vào từng buổi; đánh dấu đã xác nhận
    char a[11], b[11];
    dt_format_date(from, a);
    dt_format_date(to, b);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "from", a);
    cJSON_AddStringToObject(root, "to", b);
    cJSON *items = cJSON_AddArrayToObject(root, "items");
    for (int i = 0; i < n; i++) cJSON_AddItemToArray(items, occ_json(&occ[i]));
    cJSON_AddArrayToObject(root, "conflicts");
    free(occ);
    // TODO(TV4): JSON lớn thì gửi theo chunk thay vì tạo cả cây cJSON trong RAM
    return json_reply(req, 200, root);
}

// GET /api/calendar/week?date=YYYY-MM-DD (không có date thì dùng hôm nay của ESP; chưa có giờ thì 400)
static esp_err_t week(httpd_req_t *req)
{
    char q[16];
    int32_t day;
    if (http_query(req, "date", q, sizeof(q))) {
        if (!dt_parse_date(q, &day)) return json_error(req, 400, "BAD_ARG", "Sai ngay", "date");
    } else {
        day = time_today();
        if (day < 0) return json_error(req, 400, "BAD_ARG", "ESP chua co gio, hay gui tham so date", "date");
    }
    int32_t start = dt_week_start(day);
    return reply_range(req, start, start + 6);
}

// GET /api/calendar/range?from=&to= (tối đa SCHED_MAX_RANGE_DAYS ngày)
static esp_err_t range(httpd_req_t *req)
{
    char a[16], b[16];
    int32_t from, to;
    if (!http_query(req, "from", a, sizeof(a)) || !dt_parse_date(a, &from))
        return json_error(req, 400, "BAD_ARG", "Thieu hoac sai from", "from");
    if (!http_query(req, "to", b, sizeof(b)) || !dt_parse_date(b, &to))
        return json_error(req, 400, "BAD_ARG", "Thieu hoac sai to", "to");
    return reply_range(req, from, to);
}

// GET /api/conflicts?from=&to=
static esp_err_t conflicts_list(httpd_req_t *req) { return not_yet(req); }   // TODO(TV4)

// POST /api/conflicts/check  {"items":[{"date":"2026-09-02","start":"09:00","end":"11:00","campus":2}],"ignore":5}
static esp_err_t conflicts_check(httpd_req_t *req) { return not_yet(req); }  // TODO(TV4)

// POST /api/conflicts/ack {"a":1,"b":2,"date":"2026-09-02"}
static esp_err_t conflicts_ack(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    return not_yet(req);                                                     // TODO(TV4)
}

void calendar_register_routes(httpd_handle_t http)
{
    http_route(http, "/api/calendar/week", HTTP_GET, week);
    http_route(http, "/api/calendar/range", HTTP_GET, range);
    http_route(http, "/api/conflicts", HTTP_GET, conflicts_list);
    http_route(http, "/api/conflicts/check", HTTP_POST, conflicts_check);
    http_route(http, "/api/conflicts/ack", HTTP_POST, conflicts_ack);
}
