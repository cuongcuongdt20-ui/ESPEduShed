#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "discovery.h"

static const char *TAG = "discovery";
static void discovery_task(void *arg)
{
    (void)arg;
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) { ESP_LOGE(TAG, "socket: %d", errno); vTaskDelete(NULL); return; }
    int yes = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in local = {.sin_family=AF_INET,.sin_port=htons(5001),.sin_addr.s_addr=htonl(INADDR_ANY)};
    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) { ESP_LOGE(TAG, "bind: %d", errno); close(fd); vTaskDelete(NULL); return; }
    char buf[128]; TickType_t last_reply = 0; unsigned replies = 0;
    for (;;) {
        struct sockaddr_in peer; socklen_t plen = sizeof(peer);
        int n = recvfrom(fd, buf, sizeof(buf)-1, 0, (struct sockaddr *)&peer, &plen);
        if (n < 0) { if (errno == EINTR) continue; vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        if (n != (int)strlen("ESPEDUSHED_DISCOVER") || memcmp(buf, "ESPEDUSHED_DISCOVER", n) != 0) continue;
        TickType_t now = xTaskGetTickCount();
        if (now - last_reply >= pdMS_TO_TICKS(1000)) { last_reply = now; replies = 0; }
        if (replies >= 10) continue;
        // Ở chế độ AP, giao diện STA vẫn tồn tại nhưng IP là 0.0.0.0: lấy giao diện đầu tiên đã có IP
        static const char *const ifkeys[] = {"WIFI_STA_DEF", "WIFI_AP_DEF"};
        esp_netif_ip_info_t info = {0}; char ip[16] = "0.0.0.0";
        for (int k = 0; k < 2; k++) {
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey(ifkeys[k]);
            if (netif && esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0) {
                snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
                break;
            }
        }
        char response[160]; int len = snprintf(response, sizeof(response), "{\"name\":\"espedushed\",\"ip\":\"%s\",\"port\":5000,\"fw\":\"1.0.0\"}", ip);
        if (sendto(fd, response, len, 0, (struct sockaddr *)&peer, plen) >= 0) replies++;
    }
}
void mdns_svc_start(void);
void discovery_start(void)
{
    if (xTaskCreate(discovery_task, "udp_discovery", 3072, NULL, 4, NULL) != pdPASS) ESP_LOGE(TAG, "cannot create UDP discovery task");
    mdns_svc_start();
}
