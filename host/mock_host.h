#ifndef __MOCK_HOST_H__
#define __MOCK_HOST_H__

#include <stdint.h>

#define pdPASS 0

typedef void* xTaskHandle;

int xTaskCreate(void (*task_func)(void*), const char* const pcName,
                const uint32_t usStackDepth, void* const pvParameters,
                unsigned int uxPriority, void** pvCreatedTask);

void vTaskDelete(void* t);

#endif /* __MOCK_HOST_H__ */
