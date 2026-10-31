// Kiểm tra file sao lưu trước khi system.restore thay file thật (TV1).
#pragma once
#include <stdbool.h>

// true nếu file đúng một trong hai dạng dữ liệu trên /data: JSON Lines (dòng đầu là object có "schema", mọi dòng sau là
// object) hoặc một object JSON có "schema". Chỉ kiểm tra cấu trúc, không kiểm tra từng bản ghi: phần đó do module nạp.
bool restore_file_valid(const char *path);
