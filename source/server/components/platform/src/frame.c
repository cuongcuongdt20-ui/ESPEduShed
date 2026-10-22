// Ghép khung TCP (TV1). Xem frame.h.
#include <string.h>
#include "frame.h"

void frame_reset(frame_rx_t *rx)
{
    rx->head_have = 0;
    rx->need = 0;
    rx->have = 0;
}

void frame_put_header(uint8_t out[4], uint32_t len)
{
    out[0] = (uint8_t) (len >> 24);
    out[1] = (uint8_t) (len >> 16);
    out[2] = (uint8_t) (len >> 8);
    out[3] = (uint8_t) len;
}

int frame_feed(frame_rx_t *rx, const uint8_t *data, size_t len, frame_cb_t cb, void *user)
{
    size_t pos = 0;
    while (pos < len) {
        if (rx->head_have < 4) {
            // Bước 1: gom đủ 4 byte độ dài (có thể đến rời rạc từng byte)
            rx->head[rx->head_have++] = data[pos++];
            if (rx->head_have < 4) continue;
            rx->need = ((uint32_t) rx->head[0] << 24) | ((uint32_t) rx->head[1] << 16)
                     | ((uint32_t) rx->head[2] << 8) | rx->head[3];
            rx->have = 0;
            if (rx->need == 0 || rx->need > FRAME_MAX_PAYLOAD) return -1;
            continue;
        }
        // Bước 2: chép phần nội dung còn thiếu, nhiều nhất là phần đang có trong mẩu dữ liệu này
        size_t take = rx->need - rx->have;
        if (take > len - pos) take = len - pos;
        memcpy(rx->buf + rx->have, data + pos, take);
        rx->have += (uint32_t) take;
        pos += take;
        if (rx->have < rx->need) continue;

        // Đủ một khung: giao cho callback rồi chuẩn bị nhận khung kế tiếp (có thể nằm ngay sau trong cùng mẩu)
        rx->buf[rx->need] = 0;
        uint32_t n = rx->need;
        frame_reset(rx);
        int r = cb(user, rx->buf, n);
        if (r != 0) return r;
    }
    return 0;
}
