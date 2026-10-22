// mDNS (TV1): truy cập bằng http://<CONFIG_LD_HOSTNAME>.local
#include "esp_log.h"
#include "mdns.h"
#include "sdkconfig.h"
#include "platform_internal.h"

void mdns_start(void)
{
    if (mdns_init() != ESP_OK) {
        ESP_LOGE("mdns", "khong khoi dong duoc mDNS");
        return;
    }
    mdns_hostname_set(CONFIG_LD_HOSTNAME);
    mdns_instance_name_set("Lich giang day");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
}
