// Ghép khung TCP (TV1): 4 byte độ dài big-endian + nội dung. Logic thuần, test được trên máy tính.
//
// TCP là luồng byte: một khung có thể đến thành nhiều lần recv, hoặc nhiều khung đến trong một lần recv.
// frame_feed() nhận từng mẩu byte bất kỳ và gọi callback mỗi khi đã đủ một khung.
//
// Bộ đệm nội dung chỉ được cấp khi đã đọc xong 4 byte độ dài, đúng bằng độ dài khung, và được giải phóng ngay sau
// callback. Kết nối đang chờ không giữ bộ đệm nào, nên 8 client rảnh không chiếm 8 x 8 KB RAM.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define FRAME_MAX_PAYLOAD 8192
#define FRAME_ERR_BAD     (-1)     // độ dài 0 hoặc lớn hơn FRAME_MAX_PAYLOAD: luồng byte đã lệch
#define FRAME_ERR_NOMEM   (-2)     // không cấp được bộ đệm cho khung

typedef struct {
    uint8_t  head[4];
    uint32_t head_have;            // đã nhận bao nhiêu byte của 4 byte độ dài
    uint32_t need;                 // độ dài nội dung của khung hiện tại
    uint32_t have;                 // đã nhận bao nhiêu byte nội dung
    char    *buf;                  // need + 1 byte (cho '\0'); NULL khi chưa nhận dở khung nào
} frame_rx_t;

// Gọi với nội dung khung đã kết thúc bằng '\0'. Trả 0 để tiếp tục, khác 0 để dừng đọc (ví dụ kết nối bị đóng).
// payload bị giải phóng sau khi callback trả về; callback được phép giải phóng cả frame_rx_t.
typedef int (*frame_cb_t)(void *user, char *payload, uint32_t len);

// Bắt đầu lại từ đầu khung và giải phóng bộ đệm nếu đang nhận dở. Gọi trước khi bỏ frame_rx_t.
// frame_rx_t mới phải được đặt về 0 (memset) trước lần gọi đầu tiên.
void frame_reset(frame_rx_t *rx);

// Trả về: 0 bình thường; FRAME_ERR_BAD hoặc FRAME_ERR_NOMEM thì phải đóng kết nối (không đọc tiếp được khung đang dở);
// giá trị khác 0 do callback trả thì frame_feed dừng và trả lại đúng giá trị đó.
int frame_feed(frame_rx_t *rx, const uint8_t *data, size_t len, frame_cb_t cb, void *user);

// Ghi 4 byte độ dài big-endian
void frame_put_header(uint8_t out[4], uint32_t len);
