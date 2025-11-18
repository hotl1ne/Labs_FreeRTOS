/* FreeRTOS Kernel Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* Standard Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> /* Important for uintptr_t */

/* --- VARIANT 4 CONSTANTS --- */
#define IDX             4
#define NTASKS          4
#define BURST_N         4
#define TLS_INDEX_V1    0
#define TLS_INDEX_V2    1
#define BASE_DELAY_MS   130

/* --- STRUCTURES MUST BE EXTERNAL (Global) --- */
typedef struct {
    char name[12];
    uint32_t iter;
    uint32_t checksum;
    uint32_t delayTicksOrCycles;
    uint32_t seed;
    char line1[BURST_N][64];
    uint8_t bcnt;
} task_context_t;

typedef enum { MODE_DELAY = 0, MODE_BUSY = 1 } run_mode_t;

typedef struct {
    run_mode_t mode;
    uint8_t burstMode;
    uint32_t baseDelayMs;
    uint32_t baseCycles;
    uint8_t step;
} profile_t;

/* --- HELPER FUNCTIONS --- */

uint8_t CRC8_SAE_J11850(uint8_t start, uint8_t* data, size_t len) {
    uint8_t crc = start;
    size_t i;
    uint8_t j;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x1D;
            }
            else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void burst_flush(task_context_t* ctx) {
    int i;
    for (i = 0; i < ctx->bcnt; i++) {
        printf("%s", ctx->line1[i]);
    }
    ctx->bcnt = 0;
}

/* --- TASK (WORKER) --- */
void vWorker(void* arg) {
    /* VARIABLE DECLARATIONS ONLY HERE (AT THE BEGINNING) */
    int i;
    TickType_t xDelay;
    task_context_t* ctx;
    profile_t* prof;
    int Ni;
    int iter;
    task_context_t* myCtx;
    task_context_t* finalCtx;
    uint8_t data[2];
    char buffer[64];

    /* CODE STARTS HERE */
    i = (int)(uintptr_t)arg;
    xDelay = pdMS_TO_TICKS(BASE_DELAY_MS + 7 * i);

    /* 1. Memory allocation */
    ctx = (task_context_t*)pvPortMalloc(sizeof(task_context_t));
    prof = (profile_t*)pvPortMalloc(sizeof(profile_t));

    /* 2. Context Initialization */
    sprintf(ctx->name, "Task_%d", i); /* sprintf is safer for old MSVC than snprintf */
    ctx->iter = 0;
    ctx->checksum = 0;
    ctx->delayTicksOrCycles = xDelay;
    ctx->seed = 0xAA + i;
    ctx->bcnt = 0;

    /* 3. Profile Initialization */
    prof->mode = MODE_DELAY;
    prof->burstMode = 1;
    prof->baseDelayMs = BASE_DELAY_MS;
    prof->step = 7;

    /* 4. Store in TLS */
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_V1, ctx);
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_V2, prof);

    /* 5. Iteration calculation */
    Ni = 10 * NTASKS + IDX;

    /* 6. Main loop */
    for (iter = 0; iter < Ni; iter++) {
        myCtx = (task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, TLS_INDEX_V1);

        myCtx->iter++;

        /* Update Checksum */
        data[0] = (uint8_t)myCtx->iter;
        data[1] = (uint8_t)myCtx->seed;
        myCtx->checksum = CRC8_SAE_J11850((uint8_t)myCtx->checksum, data, 2);

        /* String formatting */
        sprintf(buffer, "[%s] Tick: %u Iter: %u Sum: 0x%02X\n",
            myCtx->name, xTaskGetTickCount(), myCtx->iter, myCtx->checksum);

        /* Burst accumulation */
        if (myCtx->bcnt < BURST_N) {
            strncpy(myCtx->line1[myCtx->bcnt], buffer, 64);
            myCtx->bcnt++;
        }

        if (myCtx->bcnt >= BURST_N) {
            burst_flush(myCtx);
        }

        vTaskDelay(xDelay);
    }

    /* 7. Completion */
    finalCtx = (task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, TLS_INDEX_V1);
    burst_flush(finalCtx);

    printf("Task finished %d\n", i);

    vPortFree(ctx);
    vPortFree(prof);
    vTaskDelete(NULL);
}

/* --- MAIN FUNCTION --- */
void main_lab2(void) {
    int i;

    /* Task creation */
    for (i = 0; i < NTASKS; ++i) {
        xTaskCreate(vWorker, "Task", configMINIMAL_STACK_SIZE + 100, (void*)(uintptr_t)i, 1, NULL);
    }

    vTaskStartScheduler();

    for (;;);
}