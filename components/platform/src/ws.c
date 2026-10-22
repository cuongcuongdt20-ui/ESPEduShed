// WebSocket /ws (TV1): chỉ dùng để báo "dữ liệu đã đổi"; dữ liệu thật lấy qua REST.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "platform.h"
#include "platform_internal.h"

static const char *TAG = "ws";

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {          // bắt tay HTTP → WebSocket
        ESP_LOGI(TAG, "client moi fd=%d", httpd_req_to_sockfd(req));
        // TODO(TV1): đếm client, vượt CONFIG_LD_WS_MAX_CLIENTS thì từ chối
        return ESP_OK;
    }
    httpd_ws_frame_t frame = {.type = HTTPD_WS_TYPE_TEXT};
    if (httpd_ws_recv_frame(req, &frame, 0) != ESP_OK) return ESP_FAIL;   // lấy độ dài
    if (frame.len == 0 || frame.len > 256) return ESP_OK;                  // client chỉ gửi tin ngắn
    uint8_t buf[257];
    frame.payload = buf;
    if (httpd_ws_recv_frame(req, &frame, frame.len) != ESP_OK) return ESP_FAIL;
    buf[frame.len] = 0;
    // TODO(TV1): xử lý tin của client nếu cần (ví dụ {"t":"ping"})
    return ESP_OK;
}

// Chạy trong task httpd (qua httpd_queue_work) để gửi an toàn
static void broadcast_work(void *arg)
{
    char *msg = arg;
    httpd_handle_t http = platform_http();
    size_t n = CONFIG_LWIP_MAX_SOCKETS;
    int fds[CONFIG_LWIP_MAX_SOCKETS];
    if (httpd_get_client_list(http, &n, fds) == ESP_OK) {
        httpd_ws_frame_t f = {.type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *) msg, .len = strlen(msg)};
        for (size_t i = 0; i < n; i++) {
            if (httpd_ws_get_fd_info(http, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET)
                httpd_ws_send_frame_async(http, fds[i], &f);
        }
    }
    free(msg);
}

void ws_notify_changed(const char *entity, uint32_t id)
{
    char *msg = malloc(96);
    if (!msg) return;
    snprintf(msg, 96, "{\"t\":\"changed\",\"entity\":\"%s\",\"id\":%lu}", entity, (unsigned long) id);
    if (httpd_queue_work(platform_http(), broadcast_work, msg) != ESP_OK) free(msg);
}

void ws_register(httpd_handle_t http)
{
    httpd_uri_t u = {.uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true};
    httpd_register_uri_handler(http, &u);
}
