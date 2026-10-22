# TV4: kiểm tra trùng lịch, API lịch tuần / khoảng ngày, xác nhận cảnh báo
idf_component_register(
    SRCS "src/conflict.c" "src/ack_store.c" "src/calendar_api.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES esp_http_server platform schedule
    PRIV_REQUIRES json catalog
)
