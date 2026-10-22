// Ghép khung TCP (TV1): 4 byte độ dài big-endian + nội dung. Logic thuần, test được trên máy tính.
//
// TCP là luồng byte: một khung có thể đến thành nhiều lần recv, hoặc nhiều khung đến trong một lần recv.
// frame_feed() nhận từng mẩu byte bất kỳ và gọi callback mỗi khi đã đủ một khung.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define FRAME_MAX_PAYLOAD 8192

typedef struct {
    uint8_t  head[4];
    uint32_t head_have;            // đã nhận bao nhiêu byte của 4 byte độ dài
    uint32_t need;                 // độ dài nội dung của khung hiện tại
    uint32_t have;                 // đã nhận bao nhiêu byte nội dung
    char     buf[FRAME_MAX_PAYLOAD + 1];   // +1 cho ký tự kết thúc chuỗi
} frame_rx_t;

// Gọi với nội dung khung đã kết thúc bằng '\0'. Trả 0 để tiếp tục, khác 0 để dừng đọc (ví dụ kết nối bị đóng).
typedef int (*frame_cb_t)(void *user, char *payload, uint32_t len);

void frame_reset(frame_rx_t *rx);

// Trả về: 0 bình thường; -1 khung hỏng (độ dài 0 hoặc lớn hơn FRAME_MAX_PAYLOAD), phải đóng kết nối vì luồng byte
// đã lệch; giá trị khác 0 do callback trả thì frame_feed dừng và trả lại đúng giá trị đó.
int frame_feed(frame_rx_t *rx, const uint8_t *data, size_t len, frame_cb_t cb, void *user);

// Ghi 4 byte độ dài big-endian
void frame_put_header(uint8_t out[4], uint32_t len);
