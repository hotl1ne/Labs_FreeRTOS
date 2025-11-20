#include <stdio.h>     
#include <string.h>     

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// --- Configuration for Variant 4 ---
#define QUEUE_LEN 5             // Queue length
#define MSG_MAX_LEN 16          // Data type in queue: char[16]
#define QUEUE_ITEM_SIZE MSG_MAX_LEN

// Queue handle must be global to be accessible by tasks
QueueHandle_t qData = NULL;

// --- Function Prototypes ---
void vProducer(void* arg);
void vConsumer(void* arg);

// ====================================================================
//                             PRODUCER TASK
// ====================================================================
void vProducer(void* arg) {
    char message[MSG_MAX_LEN];
    uint32_t count = 1;
    // Send Timeout: 100 ms (as per Variant 4 requirements)
    const TickType_t xSendTimeout = pdMS_TO_TICKS(100);

    for (;;) {
        // Generate a string: "Msg1", "Msg2", ...
        snprintf(message, MSG_MAX_LEN, "Msg%lu", (unsigned long)count);
        count++;

        // Send the string to the queue with a timeout
        // xQueueSend returns pdPASS if the item was successfully posted.
        if (xQueueSend(qData, message, xSendTimeout) == pdPASS) {
            printf("[Producer] sent: %s (tick %lu)\n", message, (unsigned long)xTaskGetTickCount());
        }
        else {
            // This is logged if the queue is full and did not clear within 100 ms
            printf("[Producer] ERROR: queue full - timeout (%lu ticks)\n", xSendTimeout);
        }

        // Producer delay - time between sends
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}


// ====================================================================
//                             CONSUMER TASK
// ====================================================================
void vConsumer(void* arg) {
    // Buffer for the received string
    char received_message[MSG_MAX_LEN];
    // Receive Timeout: 1000 ms (as per Variant 4 requirements)
    const TickType_t xReceiveTimeout = pdMS_TO_TICKS(1000);

    for (;;) {
        // Receive the string from the queue with a timeout
        // xQueueReceive returns pdPASS if the item was successfully received.
        if (xQueueReceive(qData, received_message, xReceiveTimeout) == pdPASS) {
            // String successfully received
            printf("[Consumer] got: %s (tick %lu)\n",
                received_message, (unsigned long)xTaskGetTickCount());
        }
        else {
            // This is logged if the queue is empty and no data arrived within 1000 ms
            printf("[Consumer] TIMEOUT: waiting for data (tick %lu)\n", (unsigned long)xTaskGetTickCount());
        }

        // Consumer delay - time between receive attempts
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}


// ====================================================================
//                             MAIN FUNCTION
// ====================================================================
int main_lab3(void) {

    // Step 1. Create the queue
    // Create a queue for char[16] with a length of 5 elements
    qData = xQueueCreate(QUEUE_LEN, QUEUE_ITEM_SIZE);

    // Assert the queue was created successfully
    if (qData == NULL) {
        printf("FATAL ERROR: Failed to create queue.\n");
        for (;;); // Block execution
    }

    // Step 2. Create the tasks
    // Producer Task (Priority 2)
    xTaskCreate(vProducer, "Producer", 1024, NULL, 2, NULL);
    // Consumer Task (Priority 1)
    xTaskCreate(vConsumer, "Consumer", 1024, NULL, 1, NULL);

    // Step 3. Start the scheduler
    vTaskStartScheduler();

    // Should never reach here if the scheduler starts successfully
    for (;;);
}
// --------------------------------------------------------------------