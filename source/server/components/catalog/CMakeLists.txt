# TV2: danh mục (trường, cơ sở, bảng tiết, ngày nghỉ, môn, lớp) và thư viện ngày giờ dt
idf_component_register(
    SRCS "src/dt.c" "src/catalog_core.c" "src/catalog_store.c" "src/catalog_cmd.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    PRIV_REQUIRES platform json schedule esp_timer
)
