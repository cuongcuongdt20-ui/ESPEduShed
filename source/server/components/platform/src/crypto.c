// Hàm mật mã dùng chung (TV1): HMAC-SHA256 và PBKDF2-HMAC-SHA256 trên mbedTLS (có tăng tốc SHA bằng phần cứng).
#include <stdio.h>
#include <string.h>
#include "esp_random.h"
#include "mbedtls/md.h"
#include "platform_internal.h"

void ld_random(void *buf, size_t len)
{
    esp_fill_random(buf, len);          // bộ sinh số ngẫu nhiên phần cứng (cần Wi-Fi hoặc BT bật để đạt chuẩn)
}

void ld_hex_encode(const uint8_t *in, size_t len, char *out)
{
    static const char HEX[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[2 * i] = HEX[in[i] >> 4];
        out[2 * i + 1] = HEX[in[i] & 15];
    }
    out[2 * len] = 0;
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool ld_hex_decode(const char *in, uint8_t *out, size_t len)
{
    if (!in || strlen(in) != 2 * len) return false;
    for (size_t i = 0; i < len; i++) {
        int hi = hex_val(in[2 * i]), lo = hex_val(in[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t) (hi << 4 | lo);
    }
    return true;
}

bool ld_ct_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

void ld_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len, uint8_t out[32])
{
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, key_len, msg, msg_len, out);
}

// PBKDF2 (RFC 8018) với khoá ra đúng 32 byte = một khối SHA-256:
// U1 = HMAC(P, S || 00000001), Ui = HMAC(P, U(i-1)), kết quả = U1 xor U2 xor ... xor Uc
void ld_pbkdf2_sha256(const uint8_t *pass, size_t pass_len, const uint8_t *salt, size_t salt_len,
                      uint32_t iterations, uint8_t out[32])
{
    static const uint8_t block_index[4] = {0, 0, 0, 1};
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, pass, pass_len);

    uint8_t u[32];
    mbedtls_md_hmac_update(&ctx, salt, salt_len);
    mbedtls_md_hmac_update(&ctx, block_index, 4);
    mbedtls_md_hmac_finish(&ctx, u);
    memcpy(out, u, 32);
    for (uint32_t i = 1; i < iterations; i++) {
        mbedtls_md_hmac_reset(&ctx);                 // giữ khoá đã nạp, không tính lại ipad/opad
        mbedtls_md_hmac_update(&ctx, u, 32);
        mbedtls_md_hmac_finish(&ctx, u);
        for (int j = 0; j < 32; j++) out[j] ^= u[j];
    }
    mbedtls_md_free(&ctx);
}
