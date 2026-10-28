// Đăng nhập challenge–response cho tài khoản duy nhất (TV1). Đặc tả: README mục 5.4.1, docs/protocol.md.
//
//   client → auth.challenge {user}                 server → {nonce, salt, iter}
//   client tính key = PBKDF2(mật khẩu, salt, iter), proof = HMAC-SHA256(key, nonce)
//   client → auth.login {user, nonce, proof}       server → {token, expiresAt, user, mustChangePassword}
//
// Mật khẩu không đi trên mạng. nonce dùng một lần, gắn với kết nối đã xin nó, hết hạn sau 30 giây, nên gói
// auth.login bắt được không gửi lại được (chống replay).
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "limiter.h"
#include "platform.h"
#include "platform_internal.h"
#include "session.h"

static const char *TAG = "auth";
static uint8_t s_fake_secret[32];           // sinh salt giả cho tài khoản không tồn tại, đổi mỗi lần khởi động

void auth_init(void)
{
    ld_random(s_fake_secret, sizeof(s_fake_secret));
    session_init();
    limiter_init();
}

static bool valid_name(const char *s)
{
    size_t n = s ? strlen(s) : 0;
    if (n < 3 || n > LD_NAME_MAX) return false;
    for (size_t i = 0; i < n; i++)
        if (!isalnum((unsigned char) s[i]) && s[i] != '.' && s[i] != '_' && s[i] != '-') return false;
    return true;
}

static void format_date(int32_t day, char out[11])
{
    // Đổi số ngày từ 1970-01-01 sang YYYY-MM-DD (thuật toán civil_from_days của Howard Hinnant)
    int32_t z = day + 719468;
    int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    int32_t doe = z - era * 146097;
    int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int32_t mp = (5 * doy + 2) / 153;
    unsigned d = (unsigned) (doy - (153 * mp + 2) / 5 + 1);
    unsigned m = (unsigned) (mp < 10 ? mp + 3 : mp - 9);
    unsigned y = (unsigned) (yoe + era * 400 + (m <= 2));
    // Lấy phần dư để trình biên dịch biết chắc chuỗi vừa 10 ký tự (tránh cảnh báo format-truncation)
    snprintf(out, 11, "%04u-%02u-%02u", y % 10000, m % 100, d % 100);
}

// Đọc {salt, iter, key} do app gửi khi đổi mật khẩu
static int read_secret(const cJSON *data, cJSON *out, uint8_t salt[LD_SALT_LEN], uint8_t key[LD_KEY_LEN],
                       uint32_t *iter)
{
    int it = 0;
    if (!ld_hex_decode(ld_get_str(data, "salt"), salt, LD_SALT_LEN))
        return ld_fail(out, LD_ERR_BAD_ARG, "salt phai la 32 ky tu hex", "salt");
    if (!ld_hex_decode(ld_get_str(data, "key"), key, LD_KEY_LEN))
        return ld_fail(out, LD_ERR_BAD_ARG, "key phai la 64 ky tu hex", "key");
    if (!ld_get_int(data, "iter", &it) || it < 1000 || it > 200000)
        return ld_fail(out, LD_ERR_BAD_ARG, "iter trong khoang 1000..200000", "iter");
    *iter = (uint32_t) it;
    return LD_OK;
}

// auth.challenge {user} → {nonce, salt, iter}
static int cmd_challenge(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    if (limiter_is_blocked(ctx->ip, ld_now_ms()))
        return ld_fail(out, LD_ERR_BUSY, "Sai qua nhieu lan, thu lai sau 60 giay", NULL);
    const char *name = ld_get_str(data, "user");
    if (!valid_name(name)) return ld_fail(out, LD_ERR_BAD_ARG, "Ten dang nhap 3..32 ky tu A-Z a-z 0-9 . _ -", "user");
    ld_challenge_t *ch = tcp_client_challenge(ctx->client);
    if (!ch) return ld_fail(out, LD_ERR_INTERNAL, "Ket noi da dong", NULL);

    uint8_t nonce[16];
    ld_random(nonce, sizeof(nonce));
    ld_hex_encode(nonce, sizeof(nonce), ch->nonce);
    strlcpy(ch->user, name, sizeof(ch->user));
    ch->expires_ms = ld_now_ms() + LD_CHALLENGE_TTL_MS;
    ch->used = true;

    ld_user_t u = {0};
    uint8_t salt[LD_SALT_LEN];
    uint32_t iter = LD_ITER_DEFAULT;
    if (users_find(name, &u)) {
        memcpy(salt, u.salt, LD_SALT_LEN);
        iter = u.iter;
    } else {
        // Tài khoản không tồn tại vẫn nhận salt (giả, cố định theo tên) để không dò được tên nào có thật
        uint8_t mac[32];
        ld_hmac_sha256(s_fake_secret, sizeof(s_fake_secret), (const uint8_t *) name, strlen(name), mac);
        memcpy(salt, mac, LD_SALT_LEN);
    }
    char salt_hex[2 * LD_SALT_LEN + 1];
    ld_hex_encode(salt, LD_SALT_LEN, salt_hex);
    cJSON_AddStringToObject(out, "nonce", ch->nonce);
    cJSON_AddStringToObject(out, "salt", salt_hex);
    cJSON_AddNumberToObject(out, "iter", iter);
    return LD_OK;
}

// auth.login {user, nonce, proof} → {token, expiresAt, user, mustChangePassword}
static int cmd_login(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    int64_t now = ld_now_ms();
    if (limiter_is_blocked(ctx->ip, now))
        return ld_fail(out, LD_ERR_BUSY, "Sai qua nhieu lan, thu lai sau 60 giay", NULL);
    const char *name = ld_get_str(data, "user");
    const char *nonce = ld_get_str(data, "nonce");
    const char *proof_hex = ld_get_str(data, "proof");
    ld_challenge_t *ch = tcp_client_challenge(ctx->client);
    if (!name || !nonce || !proof_hex || !ch) return ld_fail(out, LD_ERR_BAD_ARG, "Thieu user, nonce hoac proof", NULL);

    // nonce chỉ dùng được một lần: xoá ngay, kể cả khi đăng nhập sai
    bool fresh = ch->used && now < ch->expires_ms && !strcmp(ch->nonce, nonce) && !strcmp(ch->user, name);
    ch->used = false;

    ld_user_t u = {0};
    uint8_t proof[32], expected[32];
    bool ok = fresh && users_find(name, &u) && ld_hex_decode(proof_hex, proof, sizeof(proof));
    if (ok) {
        ld_hmac_sha256(u.key, LD_KEY_LEN, (const uint8_t *) nonce, strlen(nonce), expected);
        ok = ld_ct_equal(proof, expected, sizeof(proof));
    }
    if (!ok) {
        limiter_failure(ctx->ip, now);
        ESP_LOGW(TAG, "dang nhap sai: %s", name);
        // Cùng một thông báo cho mọi trường hợp: sai mật khẩu, không có tài khoản, nonce cũ hay đã hết hạn
        return ld_fail(out, LD_ERR_UNAUTHORIZED, "Sai tai khoan hoac mat khau, hoac phien dang nhap da het han", NULL);
    }

    limiter_success(ctx->ip);
    uint8_t rnd[16];
    char token[SESSION_TOKEN_LEN + 1];
    ld_random(rnd, sizeof(rnd));
    session_create(u.id, now, rnd, token);
    tcp_client_set_user(ctx->client, u.id);
    ESP_LOGI(TAG, "%s dang nhap", u.name);

    cJSON_AddStringToObject(out, "token", token);
    int32_t today = time_today();
    if (today >= 0) {
        char date[11];
        format_date(today + 7, date);
        cJSON_AddStringToObject(out, "expiresAt", date);
    } else {
        cJSON_AddNullToObject(out, "expiresAt");           // chưa có giờ thực: vẫn hết hạn sau 7 ngày chạy liên tục
    }
    cJSON *user = cJSON_AddObjectToObject(out, "user");
    cJSON_AddNumberToObject(user, "id", u.id);
    cJSON_AddStringToObject(user, "name", u.name);
    cJSON_AddBoolToObject(out, "mustChangePassword", u.must_change);
    return LD_OK;
}

// auth.logout
static int cmd_logout(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    session_remove(ctx->token);
    tcp_client_set_user(ctx->client, 0);
    return LD_OK;
}

// auth.password {salt, iter, key}: đổi mật khẩu, mọi token cũ hết hiệu lực
static int cmd_password(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    uint8_t salt[LD_SALT_LEN], key[LD_KEY_LEN];
    uint32_t iter;
    int r = read_secret(data, out, salt, key, &iter);
    if (r != LD_OK) return r;
    r = users_set_password(ctx->user_id, salt, key, iter, false);
    if (r != LD_OK) return ld_fail(out, r, "Khong luu duoc mat khau", NULL);
    session_remove_user(ctx->user_id);
    tcp_clients_logout_user(ctx->user_id);       // kể cả kết nối khác: không nhận tin đẩy bằng phiên đã huỷ nữa
    return LD_OK;
}

// auth.me → {user, mustChangePassword}: app dùng để kiểm tra token đã lưu còn hiệu lực không
static int cmd_me(const ld_ctx_t *ctx, const cJSON *data, cJSON *out)
{
    ld_user_t u = {0};
    if (!users_get(ctx->user_id, &u)) return ld_fail(out, LD_ERR_UNAUTHORIZED, "Tai khoan khong con", NULL);
    cJSON *user = cJSON_AddObjectToObject(out, "user");
    cJSON_AddNumberToObject(user, "id", u.id);
    cJSON_AddStringToObject(user, "name", u.name);
    cJSON_AddBoolToObject(out, "mustChangePassword", u.must_change);
    return LD_OK;
}

void auth_register_cmds(void)
{
    ld_register_cmd("auth.challenge", cmd_challenge, LD_ROLE_NONE);
    ld_register_cmd("auth.login", cmd_login, LD_ROLE_NONE);
    ld_register_cmd("auth.logout", cmd_logout, LD_ROLE_USER);
    ld_register_cmd("auth.me", cmd_me, LD_ROLE_USER);
    ld_register_cmd("auth.password", cmd_password, LD_ROLE_USER);
}
