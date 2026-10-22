// Phiên đăng nhập (TV1). Xem session.h.
#include <stdio.h>
#include <string.h>
#include "ld_lock.h"
#include "session.h"

typedef struct {
    bool     used;
    uint16_t user_id;
    int64_t  expires_ms;
    char     token[SESSION_TOKEN_LEN + 1];
} session_t;

static session_t s_sessions[SESSION_MAX];
static ld_lock_t s_lock;

void session_init(void)
{
    ld_lock_init(&s_lock);
    memset(s_sessions, 0, sizeof(s_sessions));
}

// So sánh hết mọi byte để thời gian không lộ token đúng được bao nhiêu ký tự
static bool same_token(const char *a, const char *b)
{
    unsigned diff = 0;
    for (int i = 0; i < SESSION_TOKEN_LEN; i++) diff |= (unsigned char) a[i] ^ (unsigned char) b[i];
    return diff == 0;
}

void session_create(uint16_t user_id, int64_t now_ms, const uint8_t rnd[16], char token_out[SESSION_TOKEN_LEN + 1])
{
    for (int i = 0; i < 16; i++) snprintf(token_out + 2 * i, 3, "%02x", rnd[i]);
    ld_lock(s_lock);
    int slot = 0;
    for (int i = 0; i < SESSION_MAX; i++) {
        if (!s_sessions[i].used || s_sessions[i].expires_ms <= now_ms) {
            slot = i;
            break;
        }
        if (s_sessions[i].expires_ms < s_sessions[slot].expires_ms) slot = i;   // đầy: bỏ phiên sắp hết hạn nhất
    }
    session_t *s = &s_sessions[slot];
    s->used = true;
    s->user_id = user_id;
    s->expires_ms = now_ms + SESSION_TTL_MS;
    memcpy(s->token, token_out, SESSION_TOKEN_LEN + 1);
    ld_unlock(s_lock);
}

bool session_lookup(const char *token, int64_t now_ms, uint16_t *user_id)
{
    if (!token || strlen(token) != SESSION_TOKEN_LEN) return false;
    bool found = false;
    ld_lock(s_lock);
    for (int i = 0; i < SESSION_MAX && !found; i++) {
        session_t *s = &s_sessions[i];
        if (!s->used || !same_token(s->token, token)) continue;
        if (s->expires_ms <= now_ms) {
            s->used = false;                    // hết hạn: dọn luôn
            break;
        }
        *user_id = s->user_id;
        found = true;
    }
    ld_unlock(s_lock);
    return found;
}

void session_remove(const char *token)
{
    if (!token || strlen(token) != SESSION_TOKEN_LEN) return;
    ld_lock(s_lock);
    for (int i = 0; i < SESSION_MAX; i++)
        if (s_sessions[i].used && same_token(s_sessions[i].token, token)) s_sessions[i].used = false;
    ld_unlock(s_lock);
}

void session_remove_user(uint16_t user_id)
{
    ld_lock(s_lock);
    for (int i = 0; i < SESSION_MAX; i++)
        if (s_sessions[i].used && s_sessions[i].user_id == user_id) s_sessions[i].used = false;
    ld_unlock(s_lock);
}

int session_count(int64_t now_ms)
{
    int n = 0;
    ld_lock(s_lock);
    for (int i = 0; i < SESSION_MAX; i++) n += s_sessions[i].used && s_sessions[i].expires_ms > now_ms;
    ld_unlock(s_lock);
    return n;
}
