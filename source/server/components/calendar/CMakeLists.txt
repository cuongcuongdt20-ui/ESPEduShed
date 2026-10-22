# TV4: kiểm tra trùng lịch, lịch tuần / khoảng ngày, xác nhận cảnh báo
idf_component_register(
    SRCS "src/conflict.c" "src/ack_store.c" "src/calendar_cmd.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES schedule
    PRIV_REQUIRES platform json catalog
)
