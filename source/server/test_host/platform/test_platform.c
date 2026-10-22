// Test của TV1: ghép khung TCP, phiên đăng nhập, chống dò mật khẩu. Chạy: make platform
// Không cần ESP32: frame.c, session.c, limiter.c là logic thuần.
#include <string.h>
#include "test.h"
#include "frame.h"
#include "limiter.h"
#include "session.h"

// ---------- ghép khung

typedef struct {
    int  count;
    char last[256];
    uint32_t lens[16];
} sink_t;

static int collect(void *user, char *payload, uint32_t len)
{
    sink_t *s = user;
    if (s->count < 16) s->lens[s->count] = len;
    s->count++;
    strncpy(s->last, payload, sizeof(s->last) - 1);
    return 0;
}

// Ghép "4 byte độ dài + nội dung" vào buf, trả độ dài đã ghi
static size_t put_frame(uint8_t *buf, const char *json)
{
    uint32_t n = (uint32_t) strlen(json);
    frame_put_header(buf, n);
    memcpy(buf + 4, json, n);
    return 4 + n;
}

static frame_rx_t rx;          // 8 KB: để tĩnh, không đặt trên stack

TEST(mot_khung_nguyen_ven)
{
    uint8_t buf[64];
    size_t n = put_frame(buf, "{\"id\":1}");
    sink_t s = {0};
    frame_reset(&rx);
    CHECK_EQ(frame_feed(&rx, buf, n, collect, &s), 0);
    CHECK_EQ(s.count, 1);
    CHECK(strcmp(s.last, "{\"id\":1}") == 0);
}

TEST(khung_bi_cat_tung_byte)
{
    // TCP có thể giao từng byte một: cả 4 byte độ dài cũng có thể bị cắt
    uint8_t buf[64];
    size_t n = put_frame(buf, "{\"cmd\":\"system.ping\"}");
    sink_t s = {0};
    frame_reset(&rx);
    for (size_t i = 0; i < n; i++) CHECK_EQ(frame_feed(&rx, buf + i, 1, collect, &s), 0);
    CHECK_EQ(s.count, 1);
    CHECK(strcmp(s.last, "{\"cmd\":\"system.ping\"}") == 0);
}

TEST(nhieu_khung_dinh_trong_mot_lan_recv)
{
    uint8_t buf[256];
    size_t n = put_frame(buf, "{\"id\":1}");
    n += put_frame(buf + n, "{\"id\":22}");
    n += put_frame(buf + n, "{\"id\":333}");
    sink_t s = {0};
    frame_reset(&rx);
    CHECK_EQ(frame_feed(&rx, buf, n, collect, &s), 0);
    CHECK_EQ(s.count, 3);
    CHECK_EQ(s.lens[1], 9);
    CHECK(strcmp(s.last, "{\"id\":333}") == 0);
}

TEST(khung_dinh_va_cat_lan_lon)
{
    // Khung 1 trọn vẹn + nửa đầu khung 2 trong lần 1, nửa sau khung 2 trong lần 2
    uint8_t buf[256];
    size_t a = put_frame(buf, "{\"id\":1}");
    size_t b = put_frame(buf + a, "{\"id\":2,\"data\":{\"x\":\"abcdef\"}}");
    size_t cut = a + 6;
    sink_t s = {0};
    frame_reset(&rx);
    CHECK_EQ(frame_feed(&rx, buf, cut, collect, &s), 0);
    CHECK_EQ(s.count, 1);
    CHECK_EQ(frame_feed(&rx, buf + cut, a + b - cut, collect, &s), 0);
    CHECK_EQ(s.count, 2);
    CHECK(strcmp(s.last, "{\"id\":2,\"data\":{\"x\":\"abcdef\"}}") == 0);
}

TEST(khung_lon_nhat_8kb)
{
    static uint8_t buf[4 + FRAME_MAX_PAYLOAD];
    frame_put_header(buf, FRAME_MAX_PAYLOAD);
    memset(buf + 4, 'a', FRAME_MAX_PAYLOAD);
    sink_t s = {0};
    frame_reset(&rx);
    CHECK_EQ(frame_feed(&rx, buf, sizeof(buf), collect, &s), 0);
    CHECK_EQ(s.count, 1);
    CHECK_EQ(s.lens[0], FRAME_MAX_PAYLOAD);
}

TEST(khung_qua_dai_hoac_rong_bi_tu_choi)
{
    uint8_t head[4];
    sink_t s = {0};
    frame_put_header(head, FRAME_MAX_PAYLOAD + 1);
    frame_reset(&rx);
    CHECK_EQ(frame_feed(&rx, head, 4, collect, &s), -1);
    frame_put_header(head, 0);
    frame_reset(&rx);
    CHECK_EQ(frame_feed(&rx, head, 4, collect, &s), -1);
    CHECK_EQ(s.count, 0);
}

// ---------- phiên đăng nhập

TEST(phien_tao_tra_huy)
{
    session_init();
    uint8_t rnd[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    char token[SESSION_TOKEN_LEN + 1];
    uint16_t uid = 0;
    session_create(7, 1000, rnd, token);
    CHECK_EQ((long) strlen(token), SESSION_TOKEN_LEN);
    CHECK(session_lookup(token, 2000, &uid) && uid == 7);
    CHECK(!session_lookup("00000000000000000000000000000000", 2000, &uid));
    CHECK(!session_lookup(token, 1000 + SESSION_TTL_MS, &uid));      // hết hạn sau 7 ngày
    session_create(7, 1000, rnd, token);
    session_remove_user(7);                                          // đổi mật khẩu: mọi token cũ mất hiệu lực
    CHECK(!session_lookup(token, 2000, &uid));
}

TEST(phien_day_thay_phien_sap_het_han)
{
    session_init();
    char token[SESSION_TOKEN_LEN + 1], first[SESSION_TOKEN_LEN + 1];
    uint8_t rnd[16] = {0};
    for (int i = 0; i < SESSION_MAX; i++) {
        rnd[0] = (uint8_t) i;
        session_create((uint16_t) (i + 1), 1000 + i, rnd, token);
        if (i == 0) strcpy(first, token);
    }
    CHECK_EQ(session_count(5000), SESSION_MAX);
    rnd[0] = 200;
    session_create(99, 9000, rnd, token);                            // đầy: bỏ phiên tạo sớm nhất
    uint16_t uid;
    CHECK(!session_lookup(first, 9000, &uid));
    CHECK(session_lookup(token, 9000, &uid) && uid == 99);
}

// ---------- chống dò mật khẩu

TEST(sai_5_lan_bi_chan_60_giay)
{
    limiter_init();
    uint32_t ip = 0x0101A8C0;                                        // 192.168.1.1
    for (int i = 0; i < LIMITER_MAX_FAILURES - 1; i++) limiter_failure(ip, 1000 + i);
    CHECK(!limiter_is_blocked(ip, 1010));
    limiter_failure(ip, 1010);
    CHECK(limiter_is_blocked(ip, 1011));
    CHECK(limiter_is_blocked(ip, 1010 + LIMITER_BLOCK_MS - 1));
    CHECK(!limiter_is_blocked(ip, 1010 + LIMITER_BLOCK_MS));
    CHECK(!limiter_is_blocked(0x0201A8C0, 1011));                    // IP khác không bị ảnh hưởng
}

TEST(sai_rai_rac_ngoai_cua_so_khong_bi_chan)
{
    limiter_init();
    uint32_t ip = 0x0301A8C0;
    for (int i = 0; i < 10; i++) limiter_failure(ip, (int64_t) i * (LIMITER_WINDOW_MS / 2 + 1000));
    CHECK(!limiter_is_blocked(ip, 10LL * LIMITER_WINDOW_MS));
}

TEST(dang_nhap_dung_xoa_so_lan_sai)
{
    limiter_init();
    uint32_t ip = 0x0401A8C0;
    for (int i = 0; i < LIMITER_MAX_FAILURES - 1; i++) limiter_failure(ip, 1000);
    limiter_success(ip);
    limiter_failure(ip, 1001);
    CHECK(!limiter_is_blocked(ip, 1002));
}

int main(void)
{
    RUN(mot_khung_nguyen_ven);
    RUN(khung_bi_cat_tung_byte);
    RUN(nhieu_khung_dinh_trong_mot_lan_recv);
    RUN(khung_dinh_va_cat_lan_lon);
    RUN(khung_lon_nhat_8kb);
    RUN(khung_qua_dai_hoac_rong_bi_tu_choi);
    RUN(phien_tao_tra_huy);
    RUN(phien_day_thay_phien_sap_het_han);
    RUN(sai_5_lan_bi_chan_60_giay);
    RUN(sai_rai_rac_ngoai_cua_so_khong_bi_chan);
    RUN(dang_nhap_dung_xoa_so_lan_sai);
    return REPORT();
}
