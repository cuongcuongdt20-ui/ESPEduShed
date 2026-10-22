// Tài khoản và vai trò (TV1), lưu /data/users.json.
// Server chỉ lưu salt và key = PBKDF2(mật khẩu, salt, iter), không bao giờ lưu mật khẩu.
// Chỉ task tcp_server gọi các hàm này (sau khi khởi động), nên không cần khoá.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "platform.h"
#include "platform_internal.h"

#define USERS_FILE "/data/users.json"
static const char *TAG = "users";

static ld_user_t s_users[LD_MAX_USERS];
static int       s_n;
static uint16_t  s_next_id = 1;

uint8_t users_role_from_str(const char *s)
{
    if (!s) return LD_ROLE_NONE;
    if (!strcmp(s, "admin")) return LD_ROLE_ADMIN;
    if (!strcmp(s, "teacher")) return LD_ROLE_TEACHER;
    if (!strcmp(s, "student")) return LD_ROLE_STUDENT;
    return LD_ROLE_NONE;
}

const char *users_role_to_str(uint8_t role)
{
    switch (role) {
    case LD_ROLE_ADMIN: return "admin";
    case LD_ROLE_TEACHER: return "teacher";
    case LD_ROLE_STUDENT: return "student";
    default: return "none";
    }
}

static int find_index_by_name(const char *name)
{
    for (int i = 0; i < s_n; i++)
        if (!strcmp(s_users[i].name, name)) return i;
    return -1;
}

static int find_index_by_id(uint16_t id)
{
    for (int i = 0; i < s_n; i++)
        if (s_users[i].id == id) return i;
    return -1;
}

static int save(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "schema", 1);
    cJSON_AddNumberToObject(root, "nextId", s_next_id);
    cJSON *arr = cJSON_AddArrayToObject(root, "users");
    for (int i = 0; i < s_n; i++) {
        const ld_user_t *u = &s_users[i];
        char salt[2 * LD_SALT_LEN + 1], key[2 * LD_KEY_LEN + 1];
        ld_hex_encode(u->salt, LD_SALT_LEN, salt);
        ld_hex_encode(u->key, LD_KEY_LEN, key);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", u->id);
        cJSON_AddStringToObject(o, "name", u->name);
        cJSON_AddStringToObject(o, "role", users_role_to_str(u->role));
        cJSON_AddStringToObject(o, "salt", salt);
        cJSON_AddStringToObject(o, "key", key);
        cJSON_AddNumberToObject(o, "iter", u->iter);
        cJSON_AddBoolToObject(o, "mustChange", u->must_change);
        cJSON_AddItemToArray(arr, o);
    }
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!text) return LD_ERR_INTERNAL;
    esp_err_t err = store_write_atomic(USERS_FILE, text, strlen(text));
    cJSON_free(text);
    return err == ESP_OK ? LD_OK : (err == ESP_ERR_NO_MEM ? LD_ERR_FULL : LD_ERR_INTERNAL);
}

static bool load(void)
{
    size_t len = 0;
    char *text = store_read_all(USERS_FILE, &len);
    if (!text) return false;
    cJSON *root = cJSON_ParseWithLength(text, len);
    free(text);
    int schema = 0;
    if (!root || !ld_get_int(root, "schema", &schema) || schema != 1) {
        ESP_LOGE(TAG, "%s hong hoac sai phien ban", USERS_FILE);
        cJSON_Delete(root);
        return false;
    }
    int next = 1;
    ld_get_int(root, "nextId", &next);
    s_next_id = (uint16_t) next;
    s_n = 0;
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "users");
    const cJSON *o;
    cJSON_ArrayForEach(o, arr) {
        if (s_n >= LD_MAX_USERS) break;
        ld_user_t *u = &s_users[s_n];
        memset(u, 0, sizeof(*u));
        int id = 0, iter = 0;
        bool must = false;
        const char *name = ld_get_str(o, "name");
        if (!ld_get_int(o, "id", &id) || !name || !ld_get_int(o, "iter", &iter)
            || !ld_hex_decode(ld_get_str(o, "salt"), u->salt, LD_SALT_LEN)
            || !ld_hex_decode(ld_get_str(o, "key"), u->key, LD_KEY_LEN)) {
            ESP_LOGW(TAG, "bo qua mot tai khoan hong");
            continue;
        }
        ld_get_bool(o, "mustChange", &must);
        u->id = (uint16_t) id;
        strlcpy(u->name, name, sizeof(u->name));
        u->role = users_role_from_str(ld_get_str(o, "role"));
        u->iter = (uint32_t) iter;
        u->must_change = must;
        s_n++;
    }
    cJSON_Delete(root);
    return s_n > 0;
}

// Lần đầu chạy: tạo admin với mật khẩu ngẫu nhiên. Mật khẩu ban đầu giữ trong NVS và in ra serial mỗi lần khởi động
// cho tới khi admin đổi mật khẩu, để người cài đặt không bị mất nó.
static void create_first_admin(void)
{
    static const char ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
    char pass[9];
    uint8_t rnd[8];
    ld_random(rnd, sizeof(rnd));
    for (int i = 0; i < 8; i++) pass[i] = ALPHABET[rnd[i] % (sizeof(ALPHABET) - 1)];
    pass[8] = 0;

    uint8_t salt[LD_SALT_LEN], key[LD_KEY_LEN];
    ld_random(salt, sizeof(salt));
    ld_pbkdf2_sha256((const uint8_t *) pass, 8, salt, sizeof(salt), LD_ITER_DEFAULT, key);
    s_n = 0;
    s_next_id = 1;
    uint16_t id;
    users_create("admin", LD_ROLE_ADMIN, salt, key, LD_ITER_DEFAULT, &id);

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
        ESP_LOGW(TAG, "  Tai khoan admin ban dau: admin / %s", pass);
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
        ESP_LOGW(TAG, "chua co tai khoan, tao admin");
        create_first_admin();
    }
    ESP_LOGI(TAG, "%d tai khoan", s_n);
    print_initial_password();
}

bool users_get(uint16_t id, ld_user_t *out)
{
    int i = find_index_by_id(id);
    if (i < 0) return false;
    *out = s_users[i];
    return true;
}

bool users_find(const char *name, ld_user_t *out)
{
    int i = name ? find_index_by_name(name) : -1;
    if (i < 0) return false;
    *out = s_users[i];
    return true;
}

int users_create(const char *name, uint8_t role, const uint8_t *salt, const uint8_t *key, uint32_t iter,
                 uint16_t *id_out)
{
    if (find_index_by_name(name) >= 0) return LD_ERR_CONFLICT;
    if (s_n >= LD_MAX_USERS) return LD_ERR_FULL;
    ld_user_t *u = &s_users[s_n];
    memset(u, 0, sizeof(*u));
    u->id = s_next_id++;
    strlcpy(u->name, name, sizeof(u->name));
    u->role = role;
    memcpy(u->salt, salt, LD_SALT_LEN);
    memcpy(u->key, key, LD_KEY_LEN);
    u->iter = iter;
    u->must_change = true;              // admin đặt mật khẩu hộ: người dùng phải đổi ở lần đăng nhập đầu
    s_n++;
    int r = save();
    if (r != LD_OK) {
        s_n--;                          // không ghi được: huỷ thay đổi trong RAM để RAM và flash khớp nhau
        return r;
    }
    if (id_out) *id_out = u->id;
    return LD_OK;
}

int users_set_password(uint16_t id, const uint8_t *salt, const uint8_t *key, uint32_t iter, bool must_change)
{
    int i = find_index_by_id(id);
    if (i < 0) return LD_ERR_NOT_FOUND;
    ld_user_t old = s_users[i];
    memcpy(s_users[i].salt, salt, LD_SALT_LEN);
    memcpy(s_users[i].key, key, LD_KEY_LEN);
    s_users[i].iter = iter;
    s_users[i].must_change = must_change;
    int r = save();
    if (r != LD_OK) {
        s_users[i] = old;
        return r;
    }
    if (s_users[i].role == LD_ROLE_ADMIN && !strcmp(s_users[i].name, "admin") && !must_change)
        forget_initial_password();
    return LD_OK;
}

int users_delete(uint16_t id)
{
    int i = find_index_by_id(id);
    if (i < 0) return LD_ERR_NOT_FOUND;
    ld_user_t old = s_users[i];
    s_users[i] = s_users[--s_n];
    int r = save();
    if (r != LD_OK) {
        s_users[s_n++] = s_users[i];    // khôi phục thứ tự cũ
        s_users[i] = old;
    }
    return r;
}

void users_list_json(cJSON *arr)
{
    for (int i = 0; i < s_n; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", s_users[i].id);
        cJSON_AddStringToObject(o, "name", s_users[i].name);
        cJSON_AddStringToObject(o, "role", users_role_to_str(s_users[i].role));
        cJSON_AddBoolToObject(o, "mustChangePassword", s_users[i].must_change);
        cJSON_AddItemToArray(arr, o);
    }
}
