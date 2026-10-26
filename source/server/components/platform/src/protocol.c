// Tầng ứng dụng (TV1): phân tích {id, cmd, token, data}, tra bảng lệnh, kiểm tra token và vai trò,
// gọi handler rồi đóng gói phản hồi {id, ok, data | error}. Chạy trong task tcp_server.
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "frame.h"
#include "platform.h"
#include "platform_internal.h"
#include "session.h"

static const char *TAG = "proto";

#define MAX_CMDS     96
#define ERR_MSG_KEY  "\x01msg"          // khoá tạm do ld_fail ghi vào out, tách ra khi đóng gói phản hồi
#define ERR_FIELD_KEY "\x01field"

typedef struct {
    const char *name;
    ld_cmd_fn   fn;
    uint8_t     min_role;
} cmd_t;

static cmd_t    s_cmds[MAX_CMDS];
static int      s_ncmds;
static uint32_t s_requests, s_errors, s_max_us;

static const char *const ERR_NAMES[] = {
    "OK", "BAD_JSON", "UNKNOWN_CMD", "BAD_ARG", "UNAUTHORIZED", "FORBIDDEN",
    "NOT_FOUND", "CONFLICT", "FULL", "BUSY", "INTERNAL",
};

const char *ld_err_name(int code)
{
    if (code < 0 || code >= (int) (sizeof(ERR_NAMES) / sizeof(ERR_NAMES[0]))) return "INTERNAL";
    return ERR_NAMES[code];
}

void ld_register_cmd(const char *name, ld_cmd_fn fn, uint8_t min_role)
{
    for (int i = 0; i < s_ncmds; i++) {
        if (strcmp(s_cmds[i].name, name) == 0) {
            ESP_LOGE(TAG, "lenh %s dang ky hai lan", name);
            return;
        }
    }
    if (s_ncmds >= MAX_CMDS) {
        ESP_LOGE(TAG, "bang lenh day, bo %s", name);
        return;
    }
    s_cmds[s_ncmds++] = (cmd_t) {.name = name, .fn = fn, .min_role = min_role};
}

int ld_fail(cJSON *out, int code, const char *message, const char *field)
{
    if (out) {
        cJSON_DeleteItemFromObjectCaseSensitive(out, ERR_MSG_KEY);
        cJSON_DeleteItemFromObjectCaseSensitive(out, ERR_FIELD_KEY);
        cJSON_AddStringToObject(out, ERR_MSG_KEY, message ? message : "");
        if (field) cJSON_AddStringToObject(out, ERR_FIELD_KEY, field);
    }
    return code == LD_OK ? LD_ERR_INTERNAL : code;
}

bool ld_get_int(const cJSON *data, const char *key, int *out)
{
    const cJSON *it = data ? cJSON_GetObjectItemCaseSensitive(data, key) : NULL;
    if (!cJSON_IsNumber(it)) return false;
    double v = it->valuedouble;
    if (v != (double) (int) v) return false;          // không nhận số thực
    *out = (int) v;
    return true;
}

int ld_page_offset(const cJSON *data)
{
    int off = 0;
    return ld_get_int(data, "offset", &off) && off > 0 ? off : 0;
}

bool ld_page_add(cJSON *arr, cJSON *item, int *budget)
{
    char *text = cJSON_PrintUnformatted(item);
    int n = text ? (int) strlen(text) + 1 : 0;
    cJSON_free(text);
    // Mục đầu tiên của trang luôn được nhận (mỗi mục đơn lẻ đều nhỏ hơn 8 KB), để trang nào cũng tiến lên
    if (!item || (n > *budget && cJSON_GetArraySize(arr) > 0)) {
        cJSON_Delete(item);
        return false;
    }
    *budget -= n;
    cJSON_AddItemToArray(arr, item);
    return true;
}

bool ld_get_bool(const cJSON *data, const char *key, bool *out)
{
    const cJSON *it = data ? cJSON_GetObjectItemCaseSensitive(data, key) : NULL;
    if (!cJSON_IsBool(it)) return false;
    *out = cJSON_IsTrue(it);
    return true;
}

const char *ld_get_str(const cJSON *data, const char *key)
{
    const cJSON *it = data ? cJSON_GetObjectItemCaseSensitive(data, key) : NULL;
    return cJSON_IsString(it) ? it->valuestring : NULL;
}

void protocol_stats(uint32_t *requests, uint32_t *errors, uint32_t *max_us)
{
    *requests = s_requests;
    *errors = s_errors;
    *max_us = s_max_us;
}

static const cmd_t *find_cmd(const char *name)
{
    for (int i = 0; i < s_ncmds; i++)
        if (strcmp(s_cmds[i].name, name) == 0) return &s_cmds[i];
    return NULL;
}

// Lệnh còn dùng được khi tài khoản đang bị bắt đổi mật khẩu
static bool allowed_before_password_change(const char *name)
{
    return !strcmp(name, "auth.password") || !strcmp(name, "auth.logout") || !strcmp(name, "auth.me")
           || !strcmp(name, "system.ping") || !strcmp(name, "system.status");
}

// Đóng gói và xếp phản hồi vào hàng đợi gửi. Lấy quyền sở hữu out.
static void reply(int client, const cJSON *id, int code, const char *msg, const char *field, cJSON *out)
{
    cJSON *root = cJSON_CreateObject();
    if (cJSON_IsNumber(id)) cJSON_AddNumberToObject(root, "id", id->valuedouble);
    else cJSON_AddNumberToObject(root, "id", 0);
    cJSON_AddBoolToObject(root, "ok", code == LD_OK);
    if (code != LD_OK) {
        s_errors++;
        cJSON *e = cJSON_AddObjectToObject(root, "error");
        cJSON_AddStringToObject(e, "code", ld_err_name(code));
        cJSON_AddStringToObject(e, "message", msg ? msg : "");
        if (field) cJSON_AddStringToObject(e, "field", field);
        else cJSON_AddNullToObject(e, "field");
    }
    if (out && (code == LD_OK || out->child)) cJSON_AddItemToObject(root, "data", out);
    else cJSON_Delete(out);

    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!text) {
        ESP_LOGE(TAG, "het bo nho khi tao phan hoi");
        return;
    }
    if (strlen(text) > FRAME_MAX_PAYLOAD) {
        // Handler trả quá nhiều dữ liệu: thay bằng lỗi để client không treo chờ phản hồi
        char small[160];
        snprintf(small, sizeof(small),
                 "{\"id\":%ld,\"ok\":false,\"error\":{\"code\":\"INTERNAL\",\"message\":\"Phan hoi qua 8 KB\","
                 "\"field\":null}}",
                 cJSON_IsNumber(id) ? (long) id->valuedouble : 0L);
        tcp_client_send(client, small);
        s_errors++;
    } else {
        tcp_client_send(client, text);
    }
    cJSON_free(text);
}

void protocol_handle(int client, uint32_t ip, char *payload, uint32_t len)
{
    int64_t t0 = esp_timer_get_time();
    s_requests++;

    cJSON *req = cJSON_ParseWithLength(payload, len);
    if (!cJSON_IsObject(req)) {
        reply(client, NULL, LD_ERR_BAD_JSON, "Noi dung khong phai JSON object", NULL, NULL);
        cJSON_Delete(req);
        return;
    }
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(req, "id");
    const char *name = ld_get_str(req, "cmd");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(req, "data");
    if (!name) {
        reply(client, id, LD_ERR_BAD_ARG, "Thieu truong cmd", "cmd", NULL);
        goto done;
    }
    if (data && !cJSON_IsObject(data) && !cJSON_IsNull(data)) {
        reply(client, id, LD_ERR_BAD_ARG, "Truong data phai la object", "data", NULL);
        goto done;
    }
    const cmd_t *cmd = find_cmd(name);
    if (!cmd) {
        reply(client, id, LD_ERR_UNKNOWN_CMD, name, NULL, NULL);
        goto done;
    }

    // Xác thực token (nếu có). Lệnh công khai vẫn nhận ctx đã đăng nhập khi token hợp lệ.
    ld_ctx_t ctx = {.client = client, .ip = ip};
    bool must_change = false;
    const char *token = ld_get_str(req, "token");
    uint16_t uid;
    ld_user_t user = {0};
    if (token && session_lookup(token, ld_now_ms(), &uid) && users_get(uid, &user)) {
        ctx.user_id = uid;
        ctx.role = user.role;
        ctx.token = token;
        must_change = user.must_change;
        tcp_client_set_user(client, uid);              // phiên đã đăng nhập: nhận tin đẩy, không bị đóng sau 30 giây
    }
    if (cmd->min_role > LD_ROLE_NONE) {
        if (ctx.user_id == 0) {
            reply(client, id, LD_ERR_UNAUTHORIZED, "Chua dang nhap hoac token het han", NULL, NULL);
            goto done;
        }
        if (must_change && !allowed_before_password_change(name)) {
            reply(client, id, LD_ERR_FORBIDDEN, "Phai doi mat khau truoc", NULL, NULL);
            goto done;
        }
        if (ctx.role < cmd->min_role) {
            reply(client, id, LD_ERR_FORBIDDEN, "Khong du quyen", NULL, NULL);
            goto done;
        }
    }

    cJSON *out = cJSON_CreateObject();
    int code = cmd->fn(&ctx, cJSON_IsObject(data) ? data : NULL, out);
    char msg[128] = "";
    char field[32] = "";
    const char *m = ld_get_str(out, ERR_MSG_KEY);
    const char *f = ld_get_str(out, ERR_FIELD_KEY);
    if (m) strlcpy(msg, m, sizeof(msg));
    if (f) strlcpy(field, f, sizeof(field));
    cJSON_DeleteItemFromObjectCaseSensitive(out, ERR_MSG_KEY);
    cJSON_DeleteItemFromObjectCaseSensitive(out, ERR_FIELD_KEY);
    reply(client, id, code, msg, f ? field : NULL, out);

done:
    cJSON_Delete(req);
    uint32_t us = (uint32_t) (esp_timer_get_time() - t0);
    if (us > s_max_us) s_max_us = us;
}
