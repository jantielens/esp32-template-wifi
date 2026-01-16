#include "rtos_task_utils.h"

#include "board_config.h"

#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

namespace {

struct TaskTrampolineCtx {
    TaskFunction_t fn;
    void* arg;
    TaskPsramAlloc alloc;
};

static void task_trampoline(void* pv) {
    auto* ctx = static_cast<TaskTrampolineCtx*>(pv);
    if (!ctx) {
        vTaskDelete(nullptr);
        return;
    }

    TaskFunction_t fn = ctx->fn;
    void* arg = ctx->arg;
    TaskPsramAlloc alloc = ctx->alloc;

    // Free ctx early to reduce internal pressure while task runs.
    heap_caps_free(ctx);

    if (fn) {
        fn(arg);
    }

    // If the task function returned, we can safely reclaim the stack and TCB.
    if (alloc.tcb) {
        heap_caps_free(alloc.tcb);
    }
    if (alloc.stack) {
        heap_caps_free(alloc.stack);
    }

    vTaskDelete(nullptr);
}

static bool create_task_fallback(
    TaskFunction_t taskFn,
    const char* name,
    uint32_t stackDepthWords,
    void* arg,
    UBaseType_t priority,
    TaskHandle_t* outHandle,
    BaseType_t coreId,
    bool pinned
) {
#if CONFIG_FREERTOS_UNICORE
    (void)coreId;
    (void)pinned;
    return xTaskCreate(taskFn, name, stackDepthWords, arg, priority, outHandle) == pdPASS;
#else
    if (pinned) {
        return xTaskCreatePinnedToCore(taskFn, name, stackDepthWords, arg, priority, outHandle, coreId) == pdPASS;
    }
    return xTaskCreate(taskFn, name, stackDepthWords, arg, priority, outHandle) == pdPASS;
#endif
}

static bool create_task_static(
    TaskFunction_t taskFn,
    const char* name,
    uint32_t stackDepthWords,
    void* arg,
    UBaseType_t priority,
    TaskHandle_t* outHandle,
    BaseType_t coreId,
    bool pinned,
    TaskPsramAlloc* outAlloc
) {
    if (outAlloc) {
        outAlloc->stack = nullptr;
        outAlloc->tcb = nullptr;
    }

#if USE_PSRAM_TASK_STACKS
    if (!psramFound()) {
        return false;
    }

    auto* stack = static_cast<StackType_t*>(heap_caps_malloc(
        stackDepthWords * sizeof(StackType_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    ));
    if (!stack) {
        return false;
    }

    // Keep the TCB in internal RAM for performance/safety; stack is the big win.
    auto* tcb = static_cast<StaticTask_t*>(heap_caps_malloc(
        sizeof(StaticTask_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
    ));
    if (!tcb) {
        heap_caps_free(stack);
        return false;
    }

    TaskHandle_t handle = nullptr;
#if CONFIG_FREERTOS_UNICORE
    (void)coreId;
    (void)pinned;
    handle = xTaskCreateStatic(taskFn, name, stackDepthWords, arg, priority, stack, tcb);
#else
    if (pinned) {
        handle = xTaskCreateStaticPinnedToCore(taskFn, name, stackDepthWords, arg, priority, stack, tcb, coreId);
    } else {
        handle = xTaskCreateStatic(taskFn, name, stackDepthWords, arg, priority, stack, tcb);
    }
#endif

    if (!handle) {
        heap_caps_free(tcb);
        heap_caps_free(stack);
        return false;
    }

    if (outHandle) {
        *outHandle = handle;
    }
    if (outAlloc) {
        outAlloc->stack = stack;
        outAlloc->tcb = tcb;
    }
    return true;
#else
    (void)taskFn;
    (void)name;
    (void)stackDepthWords;
    (void)arg;
    (void)priority;
    (void)outHandle;
    (void)coreId;
    (void)pinned;
    (void)outAlloc;
    return false;
#endif
}

} // namespace

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
) {
    if (outAlloc) {
        outAlloc->stack = nullptr;
        outAlloc->tcb = nullptr;
    }

    if (create_task_static(taskFn, name, stackDepthWords, arg, priority, outHandle, coreId, pinned, outAlloc)) {
        return true;
    }

    return create_task_fallback(taskFn, name, stackDepthWords, arg, priority, outHandle, coreId, pinned);
}

bool rtos_create_task_psram_stack_autofree(
    TaskFunction_t taskFn,
    const char* name,
    uint32_t stackDepthWords,
    void* arg,
    UBaseType_t priority,
    TaskHandle_t* outHandle,
    BaseType_t coreId,
    bool pinned
) {
    // Proper implementation: allocate stack/TCB first, then create static task with ctx as argument.
#if USE_PSRAM_TASK_STACKS
    if (psramFound()) {
        TaskPsramAlloc a{nullptr, nullptr};
        a.stack = static_cast<StackType_t*>(heap_caps_malloc(
            stackDepthWords * sizeof(StackType_t),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        ));
        if (a.stack) {
            a.tcb = static_cast<StaticTask_t*>(heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        }

        if (a.stack && a.tcb) {
            auto* ctx = static_cast<TaskTrampolineCtx*>(heap_caps_malloc(sizeof(TaskTrampolineCtx), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            if (ctx) {
                ctx->fn = taskFn;
                ctx->arg = arg;
                ctx->alloc = a;

                TaskHandle_t handle = nullptr;
#if CONFIG_FREERTOS_UNICORE
                (void)coreId;
                (void)pinned;
                handle = xTaskCreateStatic(task_trampoline, name, stackDepthWords, ctx, priority, a.stack, a.tcb);
#else
                if (pinned) {
                    handle = xTaskCreateStaticPinnedToCore(task_trampoline, name, stackDepthWords, ctx, priority, a.stack, a.tcb, coreId);
                } else {
                    handle = xTaskCreateStatic(task_trampoline, name, stackDepthWords, ctx, priority, a.stack, a.tcb);
                }
#endif

                if (handle) {
                    if (outHandle) {
                        *outHandle = handle;
                    }
                    return true;
                }

                heap_caps_free(ctx);
            }
        }

        if (a.tcb) heap_caps_free(a.tcb);
        if (a.stack) heap_caps_free(a.stack);
    }
#endif

    // Fallback: internal allocations via normal API.
    // We still use the trampoline so we preserve the "must return" contract.
    auto* ctx = static_cast<TaskTrampolineCtx*>(heap_caps_malloc(sizeof(TaskTrampolineCtx), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!ctx) {
        return false;
    }
    ctx->fn = taskFn;
    ctx->arg = arg;
    ctx->alloc.stack = nullptr;
    ctx->alloc.tcb = nullptr;

    const bool ok = create_task_fallback(task_trampoline, name, stackDepthWords, ctx, priority, outHandle, coreId, pinned);
    if (!ok) {
        heap_caps_free(ctx);
    }
    return ok;
}

void rtos_free_task_psram_alloc(TaskPsramAlloc* alloc) {
    if (!alloc) return;
    if (alloc->tcb) {
        heap_caps_free(alloc->tcb);
        alloc->tcb = nullptr;
    }
    if (alloc->stack) {
        heap_caps_free(alloc->stack);
        alloc->stack = nullptr;
    }
}
