// REST API lịch dạy, công việc, ngoại lệ (TV3).
// Thêm/sửa: kiểm tra dữ liệu → sinh buổi của bản ghi mới → conflict_check → 409 nếu trùng (trừ khi "force":true)
// → lưu → schedule_mark_dirty() → ws_notify_changed().
#include <stdlib.h>
#include "cJSON.h"
#include "platform.h"
#include "schedule.h"
#include "schedule_internal.h"
#include "conflict.h"
#include "dt.h"

static esp_err_t not_yet(httpd_req_t *req)
{
    return json_error(req, 501, "NOT_IMPLEMENTED", "Chua lam", NULL);
}

// GET /api/occurrences?from=YYYY-MM-DD&to=YYYY-MM-DD → [{sourceId,kind,status,date,origDate,start,end,school,campus}]
static esp_err_t occurrences(httpd_req_t *req)
{
    char a[16], b[16];
    int32_t from, to;
    if (!http_query(req, "from", a, sizeof(a)) || !dt_parse_date(a, &from))
        return json_error(req, 400, "BAD_ARG", "Thieu hoac sai from", "from");
    if (!http_query(req, "to", b, sizeof(b)) || !dt_parse_date(b, &to))
        return json_error(req, 400, "BAD_ARG", "Thieu hoac sai to", "to");

    const int max = 400;
    occurrence_t *occ = malloc(sizeof(occurrence_t) * max);
    if (!occ) return json_error(req, 500, "INTERNAL", "Het bo nho", NULL);
    int n = sched_expand(from, to, occ, max);
    if (n < 0) {
        free(occ);
        return json_error(req, 400, "BAD_ARG", "Khoang ngay qua dai hoac qua nhieu buoi", NULL);
    }
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        char d[11], od[11], s[6], e[6];
        dt_format_date(occ[i].day, d);
        dt_format_date(occ[i].orig_day, od);
        dt_format_hm(occ[i].start_min, s);
        dt_format_hm(occ[i].end_min, e);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "sourceId", occ[i].source_id);
        cJSON_AddNumberToObject(o, "kind", occ[i].kind);
        cJSON_AddNumberToObject(o, "status", occ[i].status);
        cJSON_AddStringToObject(o, "date", d);
        cJSON_AddStringToObject(o, "origDate", od);
        cJSON_AddStringToObject(o, "start", s);
        cJSON_AddStringToObject(o, "end", e);
        cJSON_AddNumberToObject(o, "school", occ[i].school_id);
        cJSON_AddNumberToObject(o, "campus", occ[i].campus_id);
        cJSON_AddItemToArray(arr, o);
    }
    free(occ);
    return json_reply(req, 200, arr);
}

// Lịch dạy định kỳ
static esp_err_t series_list(httpd_req_t *req) { return not_yet(req); }   // TODO(TV3)
static esp_err_t series_get(httpd_req_t *req) { return not_yet(req); }    // GET /api/schedules/{id}

static esp_err_t series_create(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    // TODO(TV3): đọc body; kiểm tra trường tồn tại (catalog_school_exists), tiết hợp lệ (catalog_period_to_time),
    //            from <= to, weekday 1..7; sinh buổi trong khoảng hiệu lực (theo từng đoạn tối đa 62 ngày);
    //            conflict_check(...); trùng và không có "force":true → 409 kèm "conflicts"
    return not_yet(req);
}

static esp_err_t series_put(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    // TODO(TV3): như create, ignore_source_id = id đang sửa; xoá ngoại lệ có ngày gốc không còn thuộc lịch,
    //            trả số ngoại lệ đã xoá trong "removedExceptions"
    return not_yet(req);
}

static esp_err_t series_delete(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    // TODO(TV3): xoá lịch và mọi ngoại lệ của nó
    return not_yet(req);
}

// Ngoại lệ: POST /api/schedules/{id}/exceptions, PUT/DELETE /api/schedules/{id}/exceptions/{YYYY-MM-DD}
// (dùng chung route "/api/schedules/*" với lịch, phân biệt bằng phần đuôi đường dẫn)
static esp_err_t series_post_sub(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    return not_yet(req);
}

// Công việc
static esp_err_t tasks_list(httpd_req_t *req) { return not_yet(req); }
static esp_err_t tasks_create(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }
static esp_err_t tasks_put(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }
static esp_err_t tasks_delete(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }

void schedule_register_routes(httpd_handle_t http)
{
    http_route(http, "/api/occurrences", HTTP_GET, occurrences);

    http_route(http, "/api/schedules", HTTP_GET, series_list);
    http_route(http, "/api/schedules", HTTP_POST, series_create);
    http_route(http, "/api/schedules/*", HTTP_GET, series_get);
    http_route(http, "/api/schedules/*", HTTP_PUT, series_put);
    http_route(http, "/api/schedules/*", HTTP_POST, series_post_sub);
    http_route(http, "/api/schedules/*", HTTP_DELETE, series_delete);

    http_route(http, "/api/tasks", HTTP_GET, tasks_list);
    http_route(http, "/api/tasks", HTTP_POST, tasks_create);
    http_route(http, "/api/tasks/*", HTTP_PUT, tasks_put);
    http_route(http, "/api/tasks/*", HTTP_DELETE, tasks_delete);
}
