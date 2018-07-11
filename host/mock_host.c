#include "mock_host.h"

int xTaskCreate(void (*task_func)(void*), const char* const pcName,
                const uint32_t usStackDepth, void* const pvParameters,
                unsigned int uxPriority, void** pvCreatedTask)
{
    return pdPASS;
}

void vTaskDelete(void* t) { (void)(t); }
