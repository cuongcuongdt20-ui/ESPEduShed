# TV3: lịch giảng dạy định kỳ, công việc, ngoại lệ buổi học, sinh buổi
idf_component_register(
    SRCS "src/schedule_core.c" "src/expand.c" "src/schedule_store.c" "src/schedule_cmd.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    PRIV_REQUIRES platform json catalog calendar esp_timer
)
