#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

struct TaskPsramAlloc {
    StackType_t* stack;
    StaticTask_t* tcb;
};

// Create a task with a PSRAM-backed stack when enabled and available.
// - When PSRAM allocation is not possible, falls back to xTaskCreate/xTaskCreatePinnedToCore.
// - When a static task is created, outAlloc receives the allocated buffers; call
//   rtos_free_task_psram_alloc() after vTaskDelete() to reclaim memory.
bool rtos_create_task_psram_stack(
    TaskFunction_t taskFn,
    const char* name,
    uint32_t stackDepthWords,
    void* arg,
    UBaseType_t priority,
    TaskHandle_t* outHandle,
    BaseType_t coreId,
    bool pinned,
    TaskPsramAlloc* outAlloc
);

// Same as above, but wraps the task function in a trampoline that frees the
// allocated buffers and deletes the task when taskFn returns.
// IMPORTANT: taskFn must return (do not call vTaskDelete(nullptr) inside it).
bool rtos_create_task_psram_stack_autofree(
    TaskFunction_t taskFn,
    const char* name,
    uint32_t stackDepthWords,
    void* arg,
    UBaseType_t priority,
    TaskHandle_t* outHandle,
    BaseType_t coreId,
    bool pinned
);

void rtos_free_task_psram_alloc(TaskPsramAlloc* alloc);
