// ld_lock.h: khoá dùng chung, chạy được cả trên ESP32 (FreeRTOS) lẫn trên máy tính khi test (không khoá).
// Mỗi module tạo một khoá riêng. Không giữ khoá khi gọi sang module khác.
#pragma once

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
typedef SemaphoreHandle_t ld_lock_t;
static inline void ld_lock_init(ld_lock_t *l) { *l = xSemaphoreCreateRecursiveMutex(); }
static inline void ld_lock(ld_lock_t l) { xSemaphoreTakeRecursive(l, portMAX_DELAY); }
static inline void ld_unlock(ld_lock_t l) { xSemaphoreGiveRecursive(l); }
#else
typedef int ld_lock_t;
static inline void ld_lock_init(ld_lock_t *l) { *l = 0; }
static inline void ld_lock(ld_lock_t l) { (void) l; }
static inline void ld_unlock(ld_lock_t l) { (void) l; }
#endif
