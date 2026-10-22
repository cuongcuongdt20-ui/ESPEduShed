// Phục vụ giao diện web từ /www (TV1). Có bản .gz thì gửi bản nén.
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "platform.h"
#include "platform_internal.h"

static const char *mime(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (!strcmp(ext, ".html")) return "text/html; charset=utf-8";
    if (!strcmp(ext, ".js")) return "text/javascript; charset=utf-8";
    if (!strcmp(ext, ".css")) return "text/css; charset=utf-8";
    if (!strcmp(ext, ".json")) return "application/json";
    if (!strcmp(ext, ".svg")) return "image/svg+xml";
    if (!strcmp(ext, ".png")) return "image/png";
    if (!strcmp(ext, ".ico")) return "image/x-icon";
    return "application/octet-stream";
}

static bool exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static esp_err_t static_handler(httpd_req_t *req)
{
    char uri[128];
    strlcpy(uri, req->uri, sizeof(uri));
    char *q = strchr(uri, '?');
    if (q) *q = 0;
    if (strstr(uri, "..")) return json_error(req, 400, "BAD_ARG", "Duong dan khong hop le", NULL);
    if (!strncmp(uri, "/api/", 5)) return json_error(req, 404, "NOT_FOUND", "Khong co API nay", NULL);
    if (!strcmp(uri, "/")) strlcpy(uri, "/index.html", sizeof(uri));

    char path[160];
    char gz[168];
    snprintf(path, sizeof(path), "/www%s", uri);
    snprintf(gz, sizeof(gz), "%s.gz", path);
    const char *file = path;
    bool gzip = false;
    if (exists(gz)) {
        file = gz;
        gzip = true;
    } else if (!exists(path)) {
        return json_error(req, 404, "NOT_FOUND", "Khong co tep", NULL);
    }

    FILE *f = fopen(file, "rb");
    if (!f) return json_error(req, 500, "INTERNAL", "Khong mo duoc tep", NULL);
    httpd_resp_set_type(req, mime(path));
    if (gzip) httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    // TODO(TV1): cache dài hạn cho tệp có phiên bản trong tên; hiện tại luôn kiểm tra lại
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    static char chunk[2048];            // chỉ task httpd dùng, không cần khoá
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, (ssize_t) n) != ESP_OK) {
            fclose(f);
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_FAIL;
        }
    }
    fclose(f);
    return httpd_resp_send_chunk(req, NULL, 0);
}

void static_register(httpd_handle_t http)
{
    http_route(http, "/*", HTTP_GET, static_handler);
}
