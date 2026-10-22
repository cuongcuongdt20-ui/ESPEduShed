# TV2: tìm server trong mạng LAN (UDP broadcast cổng 5001, mDNS espedushed.local)
idf_component_register(
    SRCS "src/udp_discovery.c" "src/mdns_svc.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES lwip freertos esp_netif platform
)
