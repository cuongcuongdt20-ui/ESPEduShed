# TV3: lịch giảng dạy định kỳ, công việc, ngoại lệ buổi học, sinh buổi
idf_component_register(
    SRCS "src/schedule_core.c" "src/expand.c" "src/schedule_store.c" "src/schedule_api.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES esp_http_server platform
    PRIV_REQUIRES json catalog calendar esp_timer
)
