// Tiện ích JSON/HTTP dùng chung cho mọi module (TV1).
#include <stdlib.h>
#include <string.h>
#include "platform.h"

static const char *status_line(int status)
{
    switch (status) {
    case 200: return "200 OK";
    case 201: return "201 Created";
    case 204: return "204 No Content";
    case 400: return "400 Bad Request";
    case 401: return "401 Unauthorized";
    case 404: return "404 Not Found";
    case 409: return "409 Conflict";
    case 413: return "413 Payload Too Large";
    case 429: return "429 Too Many Requests";
    case 501: return "501 Not Implemented";
    case 507: return "507 Insufficient Storage";
    default:  return "500 Internal Server Error";
    }
}

esp_err_t json_reply(httpd_req_t *req, int status, cJSON *body)
{
    httpd_resp_set_status(req, status_line(status));
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (!body) return httpd_resp_send(req, NULL, 0);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    char *s = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!s) {
        httpd_resp_set_status(req, status_line(500));
        return httpd_resp_send(req, NULL, 0);
    }
    esp_err_t r = httpd_resp_send(req, s, HTTPD_RESP_USE_STRLEN);
    free(s);
    return r;
}

esp_err_t json_error(httpd_req_t *req, int status, const char *code, const char *msg, const char *field)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *e = cJSON_AddObjectToObject(root, "error");
    cJSON_AddStringToObject(e, "code", code);
    cJSON_AddStringToObject(e, "message", msg ? msg : "");
    if (field) cJSON_AddStringToObject(e, "field", field);
    return json_reply(req, status, root);
}

cJSON *json_read_body(httpd_req_t *req, size_t max_len)
{
    size_t len = req->content_len;
    if (len == 0) {
        json_error(req, 400, "BAD_ARG", "Thieu noi dung JSON", NULL);
        return NULL;
    }
    if (len > max_len) {
        json_error(req, 413, "TOO_LARGE", "Noi dung qua lon", NULL);
        return NULL;
    }
    char *buf = malloc(len + 1);
    if (!buf) {
        json_error(req, 500, "INTERNAL", "Het bo nho", NULL);
        return NULL;
    }
    size_t got = 0;
    while (got < len) {
        int r = httpd_req_recv(req, buf + got, len - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) {
            free(buf);
            return NULL;                  // client đã ngắt, không trả lời được nữa
        }
        got += (size_t) r;
    }
    buf[len] = 0;
    cJSON *j = cJSON_ParseWithLength(buf, len);
    free(buf);
    if (!j) json_error(req, 400, "BAD_ARG", "JSON khong hop le", NULL);
    return j;
}

bool http_query(httpd_req_t *req, const char *key, char *out, size_t out_len)
{
    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0) return false;
    char *q = malloc(qlen + 1);
    if (!q) return false;
    bool ok = httpd_req_get_url_query_str(req, q, qlen + 1) == ESP_OK
              && httpd_query_key_value(q, key, out, out_len) == ESP_OK;
    free(q);
    return ok;
}

bool http_path_id(httpd_req_t *req, const char *prefix, uint32_t *id, const char **rest)
{
    size_t n = strlen(prefix);
    if (strncmp(req->uri, prefix, n) != 0) return false;
    const char *p = req->uri + n;
    char *end;
    unsigned long v = strtoul(p, &end, 10);
    if (end == p || v == 0 || v > UINT32_MAX) return false;
    if (*end != 0 && *end != '/' && *end != '?') return false;
    *id = (uint32_t) v;
    if (rest) *rest = end;
    return true;
}

esp_err_t http_route(httpd_handle_t http, const char *uri, httpd_method_t method,
                     esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t u = {.uri = uri, .method = method, .handler = handler, .user_ctx = NULL};
    return httpd_register_uri_handler(http, &u);
}
