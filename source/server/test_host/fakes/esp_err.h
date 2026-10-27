// esp_err.h giả cho test host (logic C không cần ESP-IDF).
#pragma once
typedef int esp_err_t;

#define ESP_OK   0
#define ESP_FAIL (-1)

const char *esp_err_to_name(esp_err_t code);
