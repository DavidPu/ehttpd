#ifndef __HOST_DEBUG_H__
#define __HOST_DEBUG_H__

#define ESP_LOGE(tag, format, ...) printf(tag ": " format "\n", ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) printf(tag ": " format "\n", ##__VA_ARGS__)
#define ESP_ERROR_CHECK(x)             \
    do {                               \
        status_t rc = (x);             \
        if (rc != STATUS_OK) {         \
            goto esp_error_check_fail; \
        }                              \
    } while (0)

#define esp_log_level_set(tag, level)
#endif /* __HOST_DEBUG_H__ */
