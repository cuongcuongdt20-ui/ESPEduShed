#include "esp_log.h"
#include "mdns.h"
static const char *TAG = "mdns_svc";
void mdns_svc_start(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { ESP_LOGE(TAG, "mdns_init: %s", esp_err_to_name(err)); return; }
    if ((err = mdns_hostname_set("espedushed")) != ESP_OK) ESP_LOGW(TAG, "hostname: %s", esp_err_to_name(err));
    mdns_instance_name_set("ESPEduShed");
    mdns_service_add(NULL, "_espedushed", "_tcp", 5000, NULL, 0);
}
