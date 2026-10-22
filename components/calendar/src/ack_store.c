// Lưu các xung đột đã xác nhận xuống /data/conflict_ack.json (TV4).
// Định dạng: {"schema":1,"acks":[{"a":1,"b":2,"date":"2026-09-02"}]}
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "platform.h"
#include "calendar_internal.h"

#define ACK_FILE "/data/conflict_ack.json"

void calendar_acks_load(void)
{
    size_t len = 0;
    char *text = store_read_all(ACK_FILE, &len);
    if (!text) return;
    cJSON *root = cJSON_ParseWithLength(text, len);
    free(text);
    // TODO(TV4): đọc mảng "acks" vào g_acks
    cJSON_Delete(root);
}

void calendar_acks_save(void)
{
    // TODO(TV4): ghi g_acks bằng store_write_atomic; bỏ các xác nhận của ngày đã qua lâu để file không phình
}

void calendar_init(void)
{
    ld_lock_init(&g_cal_lock);
    calendar_acks_load();
}
