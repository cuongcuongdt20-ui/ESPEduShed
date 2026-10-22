# TV1: mạng và nền tảng (TCP server, giao thức, đăng nhập, Wi-Fi, lưu trữ)
idf_component_register(
    SRCS "src/platform.c" "src/tcp_server.c" "src/frame.c" "src/protocol.c" "src/session.c" "src/limiter.c"
         "src/auth.c" "src/users.c" "src/crypto.c" "src/wifi.c" "src/time_svc.c" "src/storage.c"
         "src/system_cmd.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES json esp_common freertos
    PRIV_REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_timer esp_system lwip mbedtls driver esp_hw_support
                  heap
)
