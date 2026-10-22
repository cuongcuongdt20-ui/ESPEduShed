// REST API danh mục (TV2). Mọi handler ghi phải gọi auth_check_write() đầu tiên,
// kiểm tra dữ liệu, gọi catalog_mark_dirty() và ws_notify_changed() khi thành công.
#include <string.h>
#include "cJSON.h"
#include "platform.h"
#include "catalog.h"
#include "catalog_internal.h"
#include "dt.h"
#include "schedule.h"

static esp_err_t not_yet(httpd_req_t *req)
{
    return json_error(req, 501, "NOT_IMPLEMENTED", "Chua lam", NULL);
}

static cJSON *school_json(const cat_school_t *s)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "id", s->id);
    cJSON_AddStringToObject(o, "name", s->name);
    cJSON_AddStringToObject(o, "shortName", s->short_name);
    cJSON_AddStringToObject(o, "color", s->color);
    cJSON *ps = cJSON_AddArrayToObject(o, "periods");
    for (int i = 0; i < s->n_periods; i++) {
        char a[6], b[6];
        dt_format_hm(s->periods[i].start, a);
        dt_format_hm(s->periods[i].end, b);
        cJSON *p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "no", s->periods[i].no);
        cJSON_AddStringToObject(p, "start", a);
        cJSON_AddStringToObject(p, "end", b);
        cJSON_AddItemToArray(ps, p);
    }
    return o;
}

// GET /api/schools → [{id,name,shortName,color,periods:[{no,start,end}]}]
static esp_err_t schools_list(httpd_req_t *req)
{
    cJSON *arr = cJSON_CreateArray();
    ld_lock(g_cat_lock);
    for (int i = 0; i < g_cat.n_schools; i++) cJSON_AddItemToArray(arr, school_json(&g_cat.schools[i]));
    ld_unlock(g_cat_lock);
    return json_reply(req, 200, arr);
}

// POST /api/schools {name,shortName,color}
static esp_err_t schools_create(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    // TODO(TV2): đọc body, kiểm tra, thêm trường, catalog_mark_dirty(), ws_notify_changed("school", id), 201
    return not_yet(req);
}

// GET /api/schools/{id} và GET /api/schools/{id}/campuses
static esp_err_t schools_get(httpd_req_t *req)
{
    uint32_t id;
    const char *rest;
    if (!http_path_id(req, "/api/schools/", &id, &rest)) return json_error(req, 404, "NOT_FOUND", "Sai duong dan", NULL);
    // TODO(TV2): rest == "" → một trường; rest == "/campuses" → danh sách cơ sở
    return not_yet(req);
}

// PUT /api/schools/{id} | PUT /api/schools/{id}/periods (thay cả bảng tiết) | PUT .../campuses/{cid}
static esp_err_t schools_put(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    // TODO(TV2): kiểm tra bảng tiết: end > start, không chồng nhau, số thứ tự không trùng
    return not_yet(req);
}

// POST /api/schools/{id}/campuses
static esp_err_t schools_post_sub(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    return not_yet(req);
}

// DELETE /api/schools/{id} | DELETE /api/schools/{id}/campuses/{cid}
static esp_err_t schools_delete(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    // TODO(TV2): sched_uses_school(id) == true → 409 CONFLICT
    return not_yet(req);
}

// Ngày nghỉ, môn học, lớp, thời gian di chuyển: cùng một mẫu CRUD
static esp_err_t holidays_list(httpd_req_t *req) { return not_yet(req); }
static esp_err_t holidays_create(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }
static esp_err_t holidays_put(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }
static esp_err_t holidays_delete(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }

static esp_err_t subjects_list(httpd_req_t *req) { return not_yet(req); }
static esp_err_t subjects_create(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }
static esp_err_t subjects_put(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }
static esp_err_t subjects_delete(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }

static esp_err_t classes_list(httpd_req_t *req) { return not_yet(req); }
static esp_err_t classes_create(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }
static esp_err_t classes_put(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }
static esp_err_t classes_delete(httpd_req_t *req)
{
    if (!auth_check_write(req)) return ESP_OK;
    // TODO(TV2): sched_uses_class(id) == true → 409 CONFLICT
    return not_yet(req);
}

// GET /api/travel → {"default":30,"pairs":[{"a":1,"b":2,"minutes":45}]}; PUT thay cả bảng
static esp_err_t travel_get(httpd_req_t *req) { return not_yet(req); }
static esp_err_t travel_put(httpd_req_t *req) { if (!auth_check_write(req)) return ESP_OK; return not_yet(req); }

void catalog_register_routes(httpd_handle_t http)
{
    // Đường dẫn chính xác đăng ký trước đường dẫn có "*"
    http_route(http, "/api/schools", HTTP_GET, schools_list);
    http_route(http, "/api/schools", HTTP_POST, schools_create);
    http_route(http, "/api/schools/*", HTTP_GET, schools_get);
    http_route(http, "/api/schools/*", HTTP_PUT, schools_put);
    http_route(http, "/api/schools/*", HTTP_POST, schools_post_sub);
    http_route(http, "/api/schools/*", HTTP_DELETE, schools_delete);

    http_route(http, "/api/holidays", HTTP_GET, holidays_list);
    http_route(http, "/api/holidays", HTTP_POST, holidays_create);
    http_route(http, "/api/holidays/*", HTTP_PUT, holidays_put);
    http_route(http, "/api/holidays/*", HTTP_DELETE, holidays_delete);

    http_route(http, "/api/subjects", HTTP_GET, subjects_list);
    http_route(http, "/api/subjects", HTTP_POST, subjects_create);
    http_route(http, "/api/subjects/*", HTTP_PUT, subjects_put);
    http_route(http, "/api/subjects/*", HTTP_DELETE, subjects_delete);

    http_route(http, "/api/classes", HTTP_GET, classes_list);
    http_route(http, "/api/classes", HTTP_POST, classes_create);
    http_route(http, "/api/classes/*", HTTP_PUT, classes_put);
    http_route(http, "/api/classes/*", HTTP_DELETE, classes_delete);

    http_route(http, "/api/travel", HTTP_GET, travel_get);
    http_route(http, "/api/travel", HTTP_PUT, travel_put);
}
