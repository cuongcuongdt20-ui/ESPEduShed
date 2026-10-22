# TV1: nền tảng mạng và hạ tầng server
idf_component_register(
    SRCS "src/storage.c" "src/json_util.c" "src/http_server.c" "src/static_files.c"
         "src/ws.c" "src/wifi.c" "src/captive_dns.c" "src/time_svc.c" "src/mdns_svc.c"
         "src/auth.c" "src/system_api.c" "src/platform.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES esp_http_server json freertos
    PRIV_REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_timer lwip mbedtls esp_system
)
