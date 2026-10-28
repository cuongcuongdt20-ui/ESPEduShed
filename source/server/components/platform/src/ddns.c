// Cập nhật tên miền động DuckDNS (TV1), để app và điện thoại tìm được ESP32 từ mạng ngoài qua port forwarding.
//  - Cấu hình: lệnh system.ddns {domain, token} lưu NVS namespace "ddns", hoặc mặc định trong menuconfig.
//    domain rỗng thì tắt. domain là phần trước ".duckdns.org".
//  - Task riêng gửi https://www.duckdns.org/update mỗi 5 phút và ngay khi có IP mới; DuckDNS tự lấy IP công khai
//    của router từ địa chỉ nguồn của yêu cầu.
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ld_lock.h"
#include "nvs.h"
#include "platform.h"
#include "platform_internal.h"

static const char *TAG = "ddns";

#define DDNS_PERIOD_MS (5 * 60 * 1000)
#define DDNS_RETRY_MS  (30 * 1000)          // chưa cập nhật được lần nào: thử lại sau 30 giây
#define DOMAIN_MAX     63
#define TOKEN_MAX      40

static ld_lock_t    s_lock;
static char         s_domain[DOMAIN_MAX + 1];
static char         s_token[TOKEN_MAX + 1];
static TaskHandle_t s_task;
static bool         s_last_ok;
static uint32_t     s_gen;             // tăng mỗi lần đổi cấu hình: bỏ kết quả của yêu cầu gửi theo cấu hình cũ
static int64_t      s_last_ms = -1;    // lần cập nhật gần nhất (thành công hay không), -1: chưa thử

// Tên DuckDNS không phân biệt hoa thường: đổi về chữ thường rồi kiểm tra
static bool valid_domain(char *s)
{
    size_t n = strlen(s);
    if (n > DOMAIN_MAX) return false;
    for (size_t i = 0; i < n; i++) s[i] = (char) tolower((unsigned char) s[i]);
    for (size_t i = 0; i < n; i++)
        if (!islower((unsigned char) s[i]) && !isdigit((unsigned char) s[i]) && s[i] != '-') return false;
    return true;
}

static bool valid_token(const char *s)
{
    size_t n = strlen(s);
    if (n < 8 || n > TOKEN_MAX) return false;
    for (size_t i = 0; i < n; i++)
        if (!isxdigit((unsigned char) s[i]) && s[i] != '-') return false;
    return true;
}

static void load_config(void)
{
    nvs_handle_t h;
    size_t ld = sizeof(s_domain), lt = sizeof(s_token);
    bool ok = false;
    if (nvs_open("ddns", NVS_READONLY, &h) == ESP_OK) {
        ok = nvs_get_str(h, "domain", s_domain, &ld) == ESP_OK && nvs_get_str(h, "token", s_token, &lt) == ESP_OK;
        nvs_close(h);
    }
    if (!ok) {
        strlcpy(s_domain, CONFIG_LD_DDNS_DOMAIN, sizeof(s_domain));
        strlcpy(s_token, CONFIG_LD_DDNS_TOKEN, sizeof(s_token));
        if (!valid_domain(s_domain)) s_domain[0] = 0;
    }
}

typedef enum { DDNS_OFF, DDNS_OK, DDNS_FAIL } ddns_result_t;

static ddns_result_t update_once(void)
{
    char url[160], domain[DOMAIN_MAX + 1];
    ld_lock(s_lock);
    bool on = s_domain[0] && s_token[0];
    uint32_t gen = s_gen;
    strlcpy(domain, s_domain, sizeof(domain));          // bản sao để ghi log, cmd_ddns có thể sửa s_domain cùng lúc
    if (on) snprintf(url, sizeof(url), "https://www.duckdns.org/update?domains=%s&token=%s&ip=", s_domain, s_token);
    ld_unlock(s_lock);
    if (!on || wifi_is_ap()) return DDNS_OFF;

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return DDNS_FAIL;
    bool ok = false;
    char body[8] = "";
    if (esp_http_client_open(c, 0) == ESP_OK) {
        esp_http_client_fetch_headers(c);
        int n = esp_http_client_read_response(c, body, sizeof(body) - 1);
        if (n > 0) body[n] = 0;
        ok = esp_http_client_get_status_code(c) == 200 && !strncmp(body, "OK", 2);   // DuckDNS trả "OK" hoặc "KO"
    }
    esp_http_client_cleanup(c);
    ld_lock(s_lock);
    bool stale = gen != s_gen;                           // cấu hình đã đổi trong lúc chờ DuckDNS
    if (!stale) {
        s_last_ok = ok;
        s_last_ms = ld_now_ms();
    }
    ld_unlock(s_lock);
    if (stale) return DDNS_FAIL;                         // cmd_ddns đã đánh thức task: vòng sau chạy ngay
    if (ok) ESP_LOGI(TAG, "da cap nhat %s.duckdns.org", domain);
    else ESP_LOGW(TAG, "cap nhat DuckDNS that bai (%s)", body);
    return ok ? DDNS_OK : DDNS_FAIL;
}

// Thử ngay khi khởi động (sự kiện có IP có thể đã xảy ra trước khi đăng ký handler), sau đó 5 phút một lần.
// Lỗi (chưa có mạng, DuckDNS không trả lời) thì thử lại sau 30 giây. Tắt DDNS hoặc đang ở chế độ AP thì ngủ tới khi
// có IP mới hoặc đổi cấu hình (cả hai đều đánh thức task).
static void ddns_task(void *arg)
{
    for (;;) {
        ddns_result_t r = update_once();
        TickType_t wait = r == DDNS_OFF ? portMAX_DELAY : pdMS_TO_TICKS(r == DDNS_OK ? DDNS_PERIOD_MS : DDNS_RETRY_MS);
        ulTaskNotifyTake(pdTRUE, wait);
    }
}

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (s_task) xTaskNotifyGive(s_task);
}

// system.ddns {domain, token}: domain rỗng thì tắt. Không bao giờ trả token về.
static int cmd_ddns(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    const char *domain = ld_get_str(data, "domain");
    const char *token = ld_get_str(data, "token");
    if (!domain) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu domain", "domain");
    char d[DOMAIN_MAX + 13 + 1];
    strlcpy(d, domain, sizeof(d));
    size_t n = strlen(d);
    if (n > 12 && !strcmp(d + n - 12, ".duckdns.org")) d[n - 12] = 0;      // chấp nhận cả "abc.duckdns.org"
    if (!valid_domain(d)) return ld_fail(out, LD_ERR_BAD_ARG, "domain chi gom a-z 0-9 -, toi da 63 ky tu", "domain");
    if (d[0] && (!token || !valid_token(token)))
        return ld_fail(out, LD_ERR_BAD_ARG, "token DuckDNS khong hop le", "token");

    nvs_handle_t h;
    if (nvs_open("ddns", NVS_READWRITE, &h) != ESP_OK) return ld_fail(out, LD_ERR_INTERNAL, "Loi NVS", NULL);
    bool ok = nvs_set_str(h, "domain", d) == ESP_OK && nvs_set_str(h, "token", d[0] ? token : "") == ESP_OK
              && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    if (!ok) return ld_fail(out, LD_ERR_INTERNAL, "Khong luu duoc cau hinh DDNS", NULL);

    ld_lock(s_lock);
    strlcpy(s_domain, d, sizeof(s_domain));
    strlcpy(s_token, d[0] ? token : "", sizeof(s_token));
    s_last_ms = -1;
    s_last_ok = false;
    s_gen++;
    ld_unlock(s_lock);
    if (s_task) xTaskNotifyGive(s_task);                          // cập nhật ngay, không chờ 5 phút
    return LD_OK;
}

void ddns_status_json(cJSON *out)
{
    ld_lock(s_lock);
    cJSON *o = cJSON_AddObjectToObject(out, "ddns");
    cJSON_AddStringToObject(o, "domain", s_domain);
    if (s_last_ms >= 0) {
        cJSON_AddBoolToObject(o, "lastOk", s_last_ok);
        cJSON_AddNumberToObject(o, "lastAgoS", (double) ((ld_now_ms() - s_last_ms) / 1000));
    }
    ld_unlock(s_lock);
}

void ddns_init(void)
{
    ld_lock_init(&s_lock);
    load_config();
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL);
    xTaskCreate(ddns_task, "ddns", 8192, NULL, 3, &s_task);     // bắt tay TLS (mbedTLS + bundle chứng chỉ) cần >= 8 KB
    ld_register_cmd("system.ddns", cmd_ddns, LD_ROLE_USER);
}
