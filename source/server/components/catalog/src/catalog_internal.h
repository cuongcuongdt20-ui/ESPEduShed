// Mô hình dữ liệu nội bộ của TV2. Module khác không include file này.
// Giới hạn và cấu trúc là điểm khởi đầu, TV2 được đổi tuỳ thiết kế (cập nhật báo cáo phần giới hạn).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "ld_lock.h"

#define CAT_MAX_SCHOOLS  16
#define CAT_MAX_CAMPUSES 32
#define CAT_MAX_PERIODS  20      // số tiết tối đa của một trường
#define CAT_MAX_HOLIDAYS 64
#define CAT_MAX_SUBJECTS 64
#define CAT_MAX_CLASSES  128
#define CAT_MAX_TRAVEL   64

typedef struct {
    uint8_t  no;                 // số thứ tự tiết, không cần liên tục (1–5, 7–9, 13–16)
    uint16_t start, end;         // phút từ 00:00
} cat_period_t;

typedef struct {
    uint16_t     id;
    char         name[65];
    char         short_name[17];
    char         color[8];       // "#RRGGBB"
    uint8_t      n_periods;
    cat_period_t periods[CAT_MAX_PERIODS];   // luôn sắp tăng theo start
} cat_school_t;

typedef struct {
    uint16_t id, school_id;
    char     name[65];
    char     address[129];
} cat_campus_t;

typedef struct {
    uint16_t id;
    uint16_t school_id;          // 0 = áp dụng mọi trường
    int32_t  from, to;           // khoảng ngày, tính cả hai đầu
    char     name[65];
} cat_holiday_t;

typedef struct {
    uint16_t id;
    char     code[17];           // mã học phần
    char     name[65];
    uint8_t  credits;
} cat_subject_t;

typedef struct {
    uint16_t id, subject_id, school_id;
    char     code[17];           // mã lớp / nhóm học phần
    uint16_t size;               // sĩ số
} cat_class_t;

typedef struct {
    uint16_t campus_a, campus_b, minutes;
} cat_travel_t;

typedef struct {
    cat_school_t  schools[CAT_MAX_SCHOOLS];   uint8_t n_schools;
    cat_campus_t  campuses[CAT_MAX_CAMPUSES]; uint8_t n_campuses;
    cat_holiday_t holidays[CAT_MAX_HOLIDAYS]; uint8_t n_holidays;
    cat_subject_t subjects[CAT_MAX_SUBJECTS]; uint8_t n_subjects;
    cat_class_t   classes[CAT_MAX_CLASSES];   uint8_t n_classes;
    cat_travel_t  travel[CAT_MAX_TRAVEL];     uint8_t n_travel;
    uint16_t      default_travel;             // phút di chuyển mặc định giữa hai cơ sở khác nhau
    uint16_t      next_id;
} catalog_t;

extern catalog_t g_cat;
extern ld_lock_t g_cat_lock;

void catalog_core_init(void);        // tạo khoá, xoá mô hình
void catalog_seed_sample(void);      // dữ liệu mẫu 2 trường (dùng khi chưa có file và khi test)
const cat_school_t *cat_find_school(uint16_t id);   // gọi trong khoá

// Lưu trữ (catalog_store.c, chỉ trên ESP32)
bool catalog_load(void);             // false nếu chưa có file
void catalog_mark_dirty(void);       // hẹn ghi file sau 1 giây

// Lệnh TCP (catalog_cmd.c, chỉ trên ESP32)
void catalog_register_cmds(void);
