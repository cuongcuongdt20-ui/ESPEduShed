// Mô hình dữ liệu nội bộ của TV3. Module khác không include file này.
// Chỉ lưu QUY TẮC lặp và NGOẠI LỆ, không lưu từng buổi đã sinh.
// Với giới hạn dưới đây mô hình chiếm khoảng 100 KB RAM. Nếu module có PSRAM, cân nhắc đặt g_sched vào PSRAM
// (EXT_RAM_BSS_ATTR + CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY) rồi tăng giới hạn.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "ld_lock.h"

#define SCHED_MAX_SERIES     96
#define SCHED_MAX_TASKS      128
#define SCHED_MAX_EXCEPTIONS 256

typedef enum { TIME_BY_PERIOD = 0, TIME_DIRECT = 1 } time_mode_t;
typedef enum { EXC_CANCEL = 1, EXC_ROOM = 2, EXC_TIME = 3, EXC_MOVE = 4, EXC_MAKEUP = 5 } exc_type_t;

// Lịch dạy định kỳ. Giờ thực KHÔNG lưu: tính bằng catalog_period_to_time mỗi lần sinh buổi.
typedef struct {
    uint32_t id;
    uint16_t owner_id;           // ctx->user_id, luôn là LD_OWNER_ID vì chỉ có một tài khoản
    uint16_t class_id;           // 0 nếu nhập trực tiếp các trường bên dưới
    char     subject_code[17];
    char     subject_name[65];
    char     class_code[17];
    uint8_t  credits;
    uint16_t school_id, campus_id;
    char     room[17];
    uint16_t size;
    uint8_t  weekday;            // 1..7
    uint8_t  time_mode;          // time_mode_t
    uint8_t  first_period, period_count;   // khi TIME_BY_PERIOD
    uint16_t start_min, end_min;           // khi TIME_DIRECT
    int32_t  from, to;           // khoảng áp dụng
    bool     weekly;             // false = chỉ diễn ra một lần vào ngày from
    char     note[121];
    char     color[8];           // "#RRGGBB" chọn trong bảng màu của app; "" = dùng màu của trường
} sched_series_t;

typedef struct {
    uint32_t id;
    uint16_t owner_id;
    uint8_t  type;               // họp, coi thi, chấm thi, hướng dẫn, công tác, cá nhân
    char     title[65];
    char     content[121];
    int32_t  day;                // ngày (lặp: ngày bắt đầu)
    uint16_t start_min, end_min;
    int32_t  until;              // lặp hàng tuần tới ngày này; 0 = một lần
    uint16_t campus_id;
    char     place[65];
    uint8_t  status;             // 0 chưa làm, 1 đang làm, 2 xong, 3 huỷ
    char     note[121];
    char     color[8];           // "#RRGGBB"; "" = màu mặc định
} sched_task_t;

// Ngoại lệ của một buổi, khoá (series_id, orig_day)
typedef struct {
    uint32_t series_id;
    int32_t  orig_day;
    uint8_t  type;               // exc_type_t
    int32_t  new_day;            // EXC_MOVE, EXC_MAKEUP
    uint16_t new_start, new_end; // EXC_TIME, EXC_MOVE, EXC_MAKEUP
    char     new_room[17];       // EXC_ROOM
    char     note[65];
} sched_exception_t;

typedef struct {
    sched_series_t    series[SCHED_MAX_SERIES];         uint16_t n_series;
    sched_task_t      tasks[SCHED_MAX_TASKS];           uint16_t n_tasks;
    sched_exception_t exceptions[SCHED_MAX_EXCEPTIONS]; uint16_t n_exceptions;
    uint32_t          next_id;
} schedule_t;

extern schedule_t g_sched;
extern ld_lock_t  g_sched_lock;

void schedule_core_init(void);
void schedule_seed_sample(void);

// Lưu trữ (schedule_store.c, chỉ trên ESP32)
bool schedule_load(void);
void schedule_mark_dirty(void);

// Lệnh TCP (schedule_cmd.c, chỉ trên ESP32)
void schedule_register_cmds(void);
