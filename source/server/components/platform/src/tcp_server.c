// TCP server (TV1): một task FreeRTOS phục vụ nhiều client bằng select() trên socket không chặn.
//
// Vì sao select() thay vì một task cho mỗi client: mỗi task FreeRTOS cần stack riêng (8–12 KB để chạy cJSON),
// 8 client sẽ tốn gần 100 KB RAM chỉ cho stack. Với select(), một task duy nhất chờ đồng thời trên mọi socket,
// RAM thêm cho mỗi client chỉ là bộ đệm ghép khung (cấp đúng độ dài khi đang nhận dở một khung) và hàng đợi gửi.
// Các handler lệnh vì thế cũng chạy tuần tự trong task này, nên dữ liệu của server không bị hai client sửa cùng lúc.
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "frame.h"
#include "platform.h"
#include "platform_internal.h"

static const char *TAG = "tcp";

#define OUT_QUEUE_MAX_BYTES (24 * 1024)    // client đọc chậm, hàng đợi gửi vượt mức này thì ngắt client đó
#define PUSH_QUEUE_LEN      32
#define SELECT_TIMEOUT_MS   100            // chu kỳ kiểm tra tin đẩy và thời gian chờ
#define RECV_CHUNK          1024
#define MAX_READS_PER_TURN  8              // đọc tối đa 8 KB mỗi vòng để các client khác không phải chờ lâu

typedef struct out_node {
    struct out_node *next;
    size_t           len, sent;
    uint8_t          data[];               // 4 byte độ dài + nội dung
} out_node_t;

typedef struct {
    int            sock;                   // -1: ô trống
    uint32_t       ip;
    uint16_t       port;
    frame_rx_t     rx;                     // bộ đệm khung chỉ được cấp khi đang nhận dở một khung
    out_node_t    *head, *tail;
    size_t         out_bytes;
    int64_t        connected_ms, last_rx_ms;
    uint16_t       user_id;                // 0: chưa đăng nhập
    ld_challenge_t challenge;
} client_t;

typedef struct {
    char     entity[24];
    uint32_t id;
    uint16_t user_id;                      // 0: gửi mọi phiên đã đăng nhập
} push_msg_t;

static client_t      s_clients[LD_MAX_CLIENTS];
static QueueHandle_t s_push_q;
static int           s_listen = -1;

static void ip_str(uint32_t ip, char out[16])
{
    const uint8_t *b = (const uint8_t *) &ip;
    snprintf(out, 16, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
}

static void client_close(int i, const char *why)
{
    client_t *c = &s_clients[i];
    if (c->sock < 0) return;
    char ip[16];
    ip_str(c->ip, ip);
    ESP_LOGI(TAG, "dong client #%d %s:%u (%s)", i, ip, c->port, why);
    shutdown(c->sock, SHUT_RDWR);
    close(c->sock);
    frame_reset(&c->rx);
    for (out_node_t *n = c->head; n;) {
        out_node_t *next = n->next;
        free(n);
        n = next;
    }
    memset(c, 0, sizeof(*c));
    c->sock = -1;
}

bool tcp_client_send(int i, const char *json)
{
    if (i < 0 || i >= LD_MAX_CLIENTS || s_clients[i].sock < 0) return false;
    client_t *c = &s_clients[i];
    size_t len = strlen(json);
    if (len == 0 || len > FRAME_MAX_PAYLOAD) {
        ESP_LOGE(TAG, "phan hoi %u byte vuot gioi han khung", (unsigned) len);
        return false;
    }
    if (c->out_bytes + len + 4 > OUT_QUEUE_MAX_BYTES) {
        client_close(i, "hang doi gui day: client doc qua cham");
        return false;
    }
    out_node_t *n = malloc(sizeof(out_node_t) + 4 + len);
    if (!n) {
        ESP_LOGW(TAG, "het RAM, bo phan hoi %u byte cho client #%d (heap trong %u)", (unsigned) len, i,
                 (unsigned) esp_get_free_heap_size());
        return false;
    }
    n->next = NULL;
    n->len = 4 + len;
    n->sent = 0;
    frame_put_header(n->data, (uint32_t) len);
    memcpy(n->data + 4, json, len);
    if (c->tail) c->tail->next = n;
    else c->head = n;
    c->tail = n;
    c->out_bytes += n->len;
    return true;
}

void tcp_client_set_user(int i, uint16_t user_id)
{
    if (i >= 0 && i < LD_MAX_CLIENTS && s_clients[i].sock >= 0) s_clients[i].user_id = user_id;
}

ld_challenge_t *tcp_client_challenge(int i)
{
    if (i < 0 || i >= LD_MAX_CLIENTS || s_clients[i].sock < 0) return NULL;
    return &s_clients[i].challenge;
}

int tcp_client_count(void)
{
    int n = 0;
    for (int i = 0; i < LD_MAX_CLIENTS; i++) n += s_clients[i].sock >= 0;
    return n;
}

static void push_enqueue(const char *entity, uint32_t id, uint16_t user_id)
{
    if (!s_push_q || !entity) return;
    push_msg_t m = {.id = id, .user_id = user_id};
    strlcpy(m.entity, entity, sizeof(m.entity));
    if (xQueueSend(s_push_q, &m, 0) != pdTRUE) ESP_LOGW(TAG, "hang doi tin day day, bo tin %s", entity);
}

void ld_push_changed(const char *entity, uint32_t id)
{
    push_enqueue(entity, id, 0);
}

void ld_push_changed_user(uint16_t user_id, const char *entity, uint32_t id)
{
    push_enqueue(entity, id, user_id);
}

static void process_pushes(void)
{
    push_msg_t m;
    while (xQueueReceive(s_push_q, &m, 0) == pdTRUE) {
        char json[96];
        snprintf(json, sizeof(json), "{\"push\":\"changed\",\"entity\":\"%s\",\"id\":%lu}", m.entity,
                 (unsigned long) m.id);
        for (int i = 0; i < LD_MAX_CLIENTS; i++) {
            client_t *c = &s_clients[i];
            if (c->sock < 0 || c->user_id == 0) continue;
            if (m.user_id != 0 && c->user_id != m.user_id) continue;
            tcp_client_send(i, json);
        }
    }
}

// Gửi hết phần đang chờ trong hàng đợi, dừng khi socket báo đầy (EAGAIN)
static void flush_client(int i)
{
    client_t *c = &s_clients[i];
    while (c->head) {
        out_node_t *n = c->head;
        int w = send(c->sock, n->data + n->sent, n->len - n->sent, 0);
        if (w < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            client_close(i, "loi send");
            return;
        }
        n->sent += (size_t) w;
        if (n->sent < n->len) return;
        c->head = n->next;
        if (!c->head) c->tail = NULL;
        c->out_bytes -= n->len;
        free(n);
    }
}

static int on_frame(void *user, char *payload, uint32_t len)
{
    int i = *(int *) user;
    protocol_handle(i, s_clients[i].ip, payload, len);
    return s_clients[i].sock < 0 ? 1 : 0;      // handler đã đóng kết nối: dừng ghép khung
}

static void read_client(int i)
{
    client_t *c = &s_clients[i];
    uint8_t buf[RECV_CHUNK];
    for (int turn = 0; turn < MAX_READS_PER_TURN && c->sock >= 0; turn++) {
        int n = recv(c->sock, buf, sizeof(buf), 0);
        if (n == 0) {
            client_close(i, "client dong ket noi");
            return;
        }
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) client_close(i, "loi recv");
            return;
        }
        c->last_rx_ms = ld_now_ms();
        int idx = i;
        int r = frame_feed(&c->rx, buf, (size_t) n, on_frame, &idx);
        if (r == FRAME_ERR_BAD) {
            client_close(i, "khung sai do dai, luong byte da lech");
            return;
        }
        if (r == FRAME_ERR_NOMEM) {
            ESP_LOGW(TAG, "het RAM khi nhan khung %u byte (heap trong %u)", (unsigned) c->rx.need,
                     (unsigned) esp_get_free_heap_size());
            client_close(i, "het RAM");
            return;
        }
        if (r > 0) return;
    }
}

static void set_nonblocking(int s)
{
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

static void accept_client(void)
{
    struct sockaddr_in addr;
    socklen_t alen = sizeof(addr);
    int s = accept(s_listen, (struct sockaddr *) &addr, &alen);
    if (s < 0) return;

    int slot = -1;
    for (int i = 0; i < LD_MAX_CLIENTS; i++) {
        if (s_clients[i].sock < 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        // Hết chỗ: báo BUSY rồi đóng ngay, không giữ kết nối
        static const char busy[] =
            "{\"id\":0,\"ok\":false,\"error\":{\"code\":\"BUSY\",\"message\":\"Server da du ket noi\",\"field\":null}}";
        uint8_t head[4];
        frame_put_header(head, sizeof(busy) - 1);
        send(s, head, 4, 0);
        send(s, busy, sizeof(busy) - 1, 0);
        close(s);
        ESP_LOGW(TAG, "tu choi ket noi: da du %d client", LD_MAX_CLIENTS);
        return;
    }

    set_nonblocking(s);
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));       // tin nhỏ: gửi ngay, không gom
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));       // phát hiện client mất mạng không báo
#if defined(TCP_KEEPIDLE) && defined(TCP_KEEPINTVL) && defined(TCP_KEEPCNT)
    int idle = 30, intvl = 5, cnt = 3;
    setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif

    client_t *c = &s_clients[slot];
    memset(c, 0, sizeof(*c));
    c->sock = s;
    c->ip = addr.sin_addr.s_addr;
    c->port = ntohs(addr.sin_port);
    c->connected_ms = c->last_rx_ms = ld_now_ms();
    char ip[16];
    ip_str(c->ip, ip);
    ESP_LOGI(TAG, "client #%d ket noi tu %s:%u (%d/%d)", slot, ip, c->port, tcp_client_count(), LD_MAX_CLIENTS);
}

static void check_timeouts(void)
{
    int64_t now = ld_now_ms();
    for (int i = 0; i < LD_MAX_CLIENTS; i++) {
        client_t *c = &s_clients[i];
        if (c->sock < 0) continue;
        if (now - c->last_rx_ms > LD_IDLE_TIMEOUT_MS) client_close(i, "im lang qua 60 giay");
        else if (c->user_id == 0 && now - c->connected_ms > LD_UNAUTH_TIMEOUT_MS)
            client_close(i, "30 giay chua dang nhap");
    }
}

static int open_listen_socket(void)
{
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (s < 0) return -1;
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(CONFIG_LD_TCP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) != 0 || listen(s, 4) != 0) {
        close(s);
        return -1;
    }
    set_nonblocking(s);
    return s;
}

static void server_task(void *arg)
{
    while ((s_listen = open_listen_socket()) < 0) {
        ESP_LOGE(TAG, "khong mo duoc cong TCP %d, thu lai sau 1 giay", CONFIG_LD_TCP_PORT);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "TCP server lang nghe cong %d, toi da %d client", CONFIG_LD_TCP_PORT, LD_MAX_CLIENTS);

    for (;;) {
        fd_set rfds, wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(s_listen, &rfds);
        int maxfd = s_listen;
        for (int i = 0; i < LD_MAX_CLIENTS; i++) {
            int s = s_clients[i].sock;
            if (s < 0) continue;
            FD_SET(s, &rfds);
            if (s_clients[i].head) FD_SET(s, &wfds);      // chỉ chờ ghi khi có dữ liệu cần gửi
            if (s > maxfd) maxfd = s;
        }
        struct timeval tv = {.tv_sec = 0, .tv_usec = SELECT_TIMEOUT_MS * 1000};
        int n = select(maxfd + 1, &rfds, &wfds, NULL, &tv);
        if (n < 0 && errno != EINTR) {
            ESP_LOGE(TAG, "select loi errno=%d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (n > 0) {
            if (FD_ISSET(s_listen, &rfds)) accept_client();
            for (int i = 0; i < LD_MAX_CLIENTS; i++) {
                int s = s_clients[i].sock;
                if (s < 0) continue;
                if (FD_ISSET(s, &rfds)) read_client(i);
                if (s_clients[i].sock >= 0 && FD_ISSET(s, &wfds)) flush_client(i);
            }
        }
        process_pushes();
        check_timeouts();
        // Phản hồi vừa tạo trong vòng này: thử gửi ngay, không đợi vòng select sau
        for (int i = 0; i < LD_MAX_CLIENTS; i++)
            if (s_clients[i].sock >= 0 && s_clients[i].head) flush_client(i);
    }
}

void tcp_server_start(void)
{
    for (int i = 0; i < LD_MAX_CLIENTS; i++) s_clients[i].sock = -1;
    s_push_q = xQueueCreate(PUSH_QUEUE_LEN, sizeof(push_msg_t));
    xTaskCreate(server_task, "tcp_server", 12288, NULL, 5, NULL);
}
