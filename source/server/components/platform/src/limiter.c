// Chống dò mật khẩu (TV1). Xem limiter.h.
#include <string.h>
#include "ld_lock.h"
#include "limiter.h"

typedef struct {
    uint32_t ip;
    int64_t  first_ms;          // lần sai đầu tiên của cửa sổ hiện tại
    int64_t  blocked_until;
    uint8_t  failures;
    bool     used;
} entry_t;

static entry_t   s_entries[LIMITER_SLOTS];
static ld_lock_t s_lock;

void limiter_init(void)
{
    ld_lock_init(&s_lock);
    memset(s_entries, 0, sizeof(s_entries));
}

static entry_t *find(uint32_t ip)
{
    for (int i = 0; i < LIMITER_SLOTS; i++)
        if (s_entries[i].used && s_entries[i].ip == ip) return &s_entries[i];
    return NULL;
}

bool limiter_is_blocked(uint32_t ip, int64_t now_ms)
{
    ld_lock(s_lock);
    entry_t *e = find(ip);
    bool blocked = e && now_ms < e->blocked_until;
    ld_unlock(s_lock);
    return blocked;
}

void limiter_failure(uint32_t ip, int64_t now_ms)
{
    ld_lock(s_lock);
    entry_t *e = find(ip);
    if (!e) {
        // Lấy ô trống, hoặc ô có lần sai cũ nhất và không còn bị chặn
        int pick = -1;
        for (int i = 0; i < LIMITER_SLOTS; i++) {
            if (!s_entries[i].used) {
                pick = i;
                break;
            }
            if (s_entries[i].blocked_until > now_ms) continue;
            if (pick < 0 || s_entries[i].first_ms < s_entries[pick].first_ms) pick = i;
        }
        if (pick < 0) pick = 0;                 // mọi ô đều đang chặn: bỏ ô đầu
        e = &s_entries[pick];
        memset(e, 0, sizeof(*e));
        e->used = true;
        e->ip = ip;
    }
    if (e->failures == 0 || now_ms - e->first_ms > LIMITER_WINDOW_MS) {
        e->failures = 0;
        e->first_ms = now_ms;
    }
    if (++e->failures >= LIMITER_MAX_FAILURES) {
        e->blocked_until = now_ms + LIMITER_BLOCK_MS;
        e->failures = 0;
    }
    ld_unlock(s_lock);
}

void limiter_success(uint32_t ip)
{
    ld_lock(s_lock);
    entry_t *e = find(ip);
    if (e) e->used = false;
    ld_unlock(s_lock);
}
