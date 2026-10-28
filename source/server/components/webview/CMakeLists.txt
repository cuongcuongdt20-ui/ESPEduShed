# Trang web chỉ đọc xem lịch trên điện thoại (HTTP, link bí mật)
idf_component_register(
    SRCS "src/webview.c"
    INCLUDE_DIRS "include"
    EMBED_TXTFILES "web/calendar.html"
    EMBED_FILES "web/fullcalendar.js.gz"
    PRIV_REQUIRES platform calendar schedule catalog json esp_http_server nvs_flash esp_hw_support
)
