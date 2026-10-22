// Hàm nội bộ của TV1. Module khác chỉ dùng platform.h.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_err.h"
#include "sdkconfig.h"

#define LD_FW_VERSION      "1.0.0"
#define LD_MAX_CLIENTS     CONFIG_LD_MAX_CLIENTS
#define LD_UNAUTH_TIMEOUT_MS 30000      // kết nối 30 giây chưa đăng nhập thì đóng
#define LD_IDLE_TIMEOUT_MS   60000      // im lặng 60 giây thì đóng
#define LD_CHALLENGE_TTL_MS  30000      // nonce hết hạn sau 30 giây

int64_t ld_now_ms(void);                // đồng hồ đơn điệu, ms từ lúc khởi động

// ---------- tcp_server.c
typedef struct {
    bool    used;
    char    nonce[33];                  // 16 byte hex
    char    user[33];
    int64_t expires_ms;
} ld_challenge_t;

void            tcp_server_start(void);
bool            tcp_client_send(int client, const char *json);   // xếp một khung vào hàng đợi gửi
void            tcp_client_set_user(int client, uint16_t user_id); // phiên đã đăng nhập: nhận tin đẩy
ld_challenge_t *tcp_client_challenge(int client);
int             tcp_client_count(void);

// ---------- protocol.c
void protocol_handle(int client, uint32_t ip, char *payload, uint32_t len);
void protocol_stats(uint32_t *requests, uint32_t *errors, uint32_t *max_us);

// ---------- crypto.c
void ld_random(void *buf, size_t len);
void ld_hex_encode(const uint8_t *in, size_t len, char *out);          // out cần 2*len+1 byte
bool ld_hex_decode(const char *in, uint8_t *out, size_t len);          // in phải dài đúng 2*len ký tự hex
void ld_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len, uint8_t out[32]);
void ld_pbkdf2_sha256(const uint8_t *pass, size_t pass_len, const uint8_t *salt, size_t salt_len,
                      uint32_t iterations, uint8_t out[32]);
bool ld_ct_equal(const uint8_t *a, const uint8_t *b, size_t len);      // so sánh thời gian hằng

// ---------- users.c
#define LD_MAX_USERS     32
#define LD_NAME_MAX      32
#define LD_SALT_LEN      16
#define LD_KEY_LEN       32
#define LD_ITER_DEFAULT  10000

typedef struct {
    uint16_t id;
    char     name[LD_NAME_MAX + 1];
    uint8_t  role;
    uint8_t  salt[LD_SALT_LEN];
    uint8_t  key[LD_KEY_LEN];
    uint32_t iter;
    bool     must_change;
} ld_user_t;

void        users_init(void);
bool        users_get(uint16_t id, ld_user_t *out);
bool        users_find(const char *name, ld_user_t *out);
int         users_create(const char *name, uint8_t role, const uint8_t *salt, const uint8_t *key, uint32_t iter,
                         uint16_t *id_out);
int         users_set_password(uint16_t id, const uint8_t *salt, const uint8_t *key, uint32_t iter, bool must_change);
int         users_delete(uint16_t id);
void        users_list_json(cJSON *arr);
uint8_t     users_role_from_str(const char *s);   // LD_ROLE_NONE nếu sai
const char *users_role_to_str(uint8_t role);

// ---------- auth.c, wifi.c, system_cmd.c: đăng ký lệnh
void auth_init(void);
void auth_register_cmds(void);
void wifi_init(void);
bool wifi_is_ap(void);
void wifi_ip_str(char out[16]);
bool wifi_rssi(int8_t *rssi);
void wifi_register_cmds(void);
void system_register_cmds(void);

// ---------- storage.c, time_svc.c
esp_err_t storage_init(void);
bool      storage_info(size_t *total, size_t *used);
void      time_init(void);
