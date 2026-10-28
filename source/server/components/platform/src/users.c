// Tài khoản duy nhất của hệ thống (TV1), lưu /data/users.json.
// Server chỉ lưu salt và key = PBKDF2(mật khẩu, salt, iter), không bao giờ lưu mật khẩu.
// Chỉ task tcp_server gọi các hàm này (sau khi khởi động), nên không cần khoá.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "platform.h"
#include "platform_internal.h"

#define USERS_FILE   "/data/users.json"
#define USERS_SCHEMA 2
static const char *TAG = "users";

static ld_user_t s_user;          // id 0: chưa có tài khoản
static bool      s_file_bad;      // có users.json nhưng không đọc được

static int save(void)
{
    char salt[2 * LD_SALT_LEN + 1], key[2 * LD_KEY_LEN + 1];
    ld_hex_encode(s_user.salt, LD_SALT_LEN, salt);
    ld_hex_encode(s_user.key, LD_KEY_LEN, key);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "schema", USERS_SCHEMA);
    cJSON *o = cJSON_AddObjectToObject(root, "user");
    cJSON_AddStringToObject(o, "name", s_user.name);
    cJSON_AddStringToObject(o, "salt", salt);
    cJSON_AddStringToObject(o, "key", key);
    cJSON_AddNumberToObject(o, "iter", s_user.iter);
    cJSON_AddBoolToObject(o, "mustChange", s_user.must_change);
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!text) return LD_ERR_INTERNAL;
    esp_err_t err = store_write_atomic(USERS_FILE, text, strlen(text));
    cJSON_free(text);
    return err == ESP_OK ? LD_OK : (err == ESP_ERR_NO_MEM ? LD_ERR_FULL : LD_ERR_INTERNAL);
}

static bool parse_user(const cJSON *o, ld_user_t *u)
{
    int iter = 0;
    bool must = false;
    memset(u, 0, sizeof(*u));
    if (!ld_get_int(o, "iter", &iter) || !ld_hex_decode(ld_get_str(o, "salt"), u->salt, LD_SALT_LEN)
        || !ld_hex_decode(ld_get_str(o, "key"), u->key, LD_KEY_LEN))
        return false;
    ld_get_bool(o, "mustChange", &must);
    u->id = LD_OWNER_ID;
    strlcpy(u->name, LD_USER_NAME, sizeof(u->name));
    u->iter = (uint32_t) iter;
    u->must_change = must;
    return true;
}

// schema 2: {"user": {...}}. schema 1 (bản nhiều tài khoản): giữ một tài khoản, ưu tiên tên "admin", rồi tài khoản
// vai trò admin đầu tiên, rồi tài khoản đầu tiên; bỏ các tài khoản khác. Mật khẩu của tài khoản được giữ vẫn dùng được.
static bool load(void)
{
    size_t len = 0;
    char *text = store_read_all(USERS_FILE, &len);
    if (!text) return false;
    s_file_bad = true;
    cJSON *root = cJSON_ParseWithLength(text, len);
    free(text);
    int schema = 0;
    bool ok = false;
    if (root && ld_get_int(root, "schema", &schema)) {
        if (schema == USERS_SCHEMA) {
            ok = parse_user(cJSON_GetObjectItemCaseSensitive(root, "user"), &s_user);
        } else if (schema == 1) {
            const cJSON *o, *by_name = NULL, *by_role = NULL, *first = NULL;
            cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(root, "users")) {
                const char *name = ld_get_str(o, "name");
                const char *role = ld_get_str(o, "role");
                ld_user_t tmp;
                if (!parse_user(o, &tmp)) continue;
                if (!first) first = o;
                if (!by_role && role && !strcmp(role, "admin")) by_role = o;
                if (!by_name && name && !strcmp(name, LD_USER_NAME)) by_name = o;
            }
            const cJSON *pick = by_name ? by_name : by_role ? by_role : first;
            if (pick) {
                ok = parse_user(pick, &s_user);
                ESP_LOGW(TAG, "giu tai khoan \"%s\", tu nay dang nhap ten %s", ld_get_str(pick, "name"), LD_USER_NAME);
            }
            if (ok) {
                ESP_LOGW(TAG, "chuyen %s sang ban mot tai khoan", USERS_FILE);
                if (save() != LD_OK) ESP_LOGE(TAG, "khong ghi duoc %s ban moi, se chuyen lai lan sau", USERS_FILE);
            }
        }
    }
    if (ok) s_file_bad = false;
    else ESP_LOGE(TAG, "%s hong hoac khong co tai khoan %s", USERS_FILE, LD_USER_NAME);
    cJSON_Delete(root);
    return ok;
}

// Lần đầu chạy: tạo tài khoản với mật khẩu ngẫu nhiên. Mật khẩu ban đầu giữ trong NVS và in ra serial mỗi lần
// khởi động cho tới khi đổi mật khẩu, để người cài đặt không bị mất nó.
static void create_first_user(void)
{
    static const char ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
    char pass[9];
    uint8_t rnd[8];
    ld_random(rnd, sizeof(rnd));
    for (int i = 0; i < 8; i++) pass[i] = ALPHABET[rnd[i] % (sizeof(ALPHABET) - 1)];
    pass[8] = 0;

    memset(&s_user, 0, sizeof(s_user));
    s_user.id = LD_OWNER_ID;
    strlcpy(s_user.name, LD_USER_NAME, sizeof(s_user.name));
    ld_random(s_user.salt, sizeof(s_user.salt));
    ld_pbkdf2_sha256((const uint8_t *) pass, 8, s_user.salt, sizeof(s_user.salt), LD_ITER_DEFAULT, s_user.key);
    s_user.iter = LD_ITER_DEFAULT;
    s_user.must_change = true;
    if (save() != LD_OK) {
        // Không ghi được (flash đầy, lỗi LittleFS): vẫn dùng được tới lần khởi động sau, khi đó sẽ tạo mật khẩu mới
        ESP_LOGE(TAG, "khong ghi duoc %s, mat khau nay chi dung toi khi khoi dong lai", USERS_FILE);
    }

    nvs_handle_t h;
    if (nvs_open("auth", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "init_pw", pass);
        nvs_commit(h);
        nvs_close(h);
    }
}

static void print_initial_password(void)
{
    nvs_handle_t h;
    char pass[16];
    size_t len = sizeof(pass);
    if (nvs_open("auth", NVS_READONLY, &h) != ESP_OK) return;
    if (nvs_get_str(h, "init_pw", pass, &len) == ESP_OK) {
        ESP_LOGW(TAG, "==================================================");
        ESP_LOGW(TAG, "  Mat khau ban dau: %s", pass);
        ESP_LOGW(TAG, "  Doi mat khau ngay o lan dang nhap dau tien.");
        ESP_LOGW(TAG, "==================================================");
    }
    nvs_close(h);
}

static void forget_initial_password(void)
{
    nvs_handle_t h;
    if (nvs_open("auth", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_key(h, "init_pw");
    nvs_commit(h);
    nvs_close(h);
}

void users_init(void)
{
    if (!load()) {
        if (s_file_bad) {
            // Giữ lại file hỏng để còn cứu được (khôi phục bản sao lưu khác), rồi mới tạo tài khoản mới
            remove(USERS_FILE ".bad");
            if (rename(USERS_FILE, USERS_FILE ".bad") == 0)
                ESP_LOGE(TAG, "da doi ten file hong thanh %s.bad", USERS_FILE);
        }
        ESP_LOGW(TAG, "chua co tai khoan, tao tai khoan moi voi mat khau ngau nhien");
        create_first_user();
    }
    print_initial_password();
}

bool users_get(uint16_t id, ld_user_t *out)
{
    if (s_user.id == 0 || id != s_user.id) return false;
    *out = s_user;
    return true;
}

bool users_find(const char *name, ld_user_t *out)
{
    if (s_user.id == 0 || !name || strcmp(name, s_user.name)) return false;
    *out = s_user;
    return true;
}

int users_set_password(uint16_t id, const uint8_t *salt, const uint8_t *key, uint32_t iter, bool must_change)
{
    if (s_user.id == 0 || id != s_user.id) return LD_ERR_NOT_FOUND;
    ld_user_t old = s_user;
    memcpy(s_user.salt, salt, LD_SALT_LEN);
    memcpy(s_user.key, key, LD_KEY_LEN);
    s_user.iter = iter;
    s_user.must_change = must_change;
    int r = save();
    if (r != LD_OK) {
        s_user = old;                   // không ghi được: huỷ thay đổi trong RAM để RAM và flash khớp nhau
        return r;
    }
    if (!must_change) forget_initial_password();
    return LD_OK;
}
