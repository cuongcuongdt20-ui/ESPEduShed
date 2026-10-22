# Dự án ESP-IDF: hệ thống quản lý lịch giảng dạy, ESP32-S3 làm web server
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lichday)

# Đóng gói thư mục web/ thành ảnh LittleFS, nạp vào phân vùng "www" mỗi lần idf.py flash.
# Phân vùng "data" (dữ liệu người dùng) KHÔNG bị ghi đè khi flash.
littlefs_create_partition_image(www web FLASH_IN_PROJECT)
