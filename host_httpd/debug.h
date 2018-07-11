#define ESP_LOGE( tag, format, ...) printf(tag ": " format, ##__VA_ARGS__)
#define ESP_LOGI ESP_LOGE
