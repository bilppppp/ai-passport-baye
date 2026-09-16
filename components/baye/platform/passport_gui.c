#include "passport_gui.h"
#include "passport_display.h"
#include <string.h>
#include <stdio.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
static const char *TAG = "baye_gui";
static QueueHandle_t s_msg_queue = NULL;
#else
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
static const char *TAG = "baye_gui";
#define HOST_QUEUE_CAP 32
static MsgType s_host_queue[HOST_QUEUE_CAP];
static int s_host_q_head = 0;
static int s_host_q_tail = 0;
static int s_host_q_count = 0;
#endif

U8 GuiInit(void) {
#ifdef ESP_PLATFORM
    if (!s_msg_queue) {
        s_msg_queue = xQueueCreate(32, sizeof(MsgType));
        if (!s_msg_queue) {
            ESP_LOGE(TAG, "Failed to create FreeRTOS message queue");
            return 0;
        }
    }
#else
    s_host_q_head = 0;
    s_host_q_tail = 0;
    s_host_q_count = 0;
#endif
    ESP_LOGI(TAG, "GUI message queue initialized");
    return 1;
}

U8 GuiPushMsg(PtrMsg pMsg) {
    if (!pMsg) return 0;
#ifdef ESP_PLATFORM
    if (!s_msg_queue) return 0;

    if (xPortInIsrContext()) {
        BaseType_t high_task_woken = pdFALSE;
        BaseType_t ret = xQueueSendFromISR(s_msg_queue, pMsg, &high_task_woken);
        if (high_task_woken) {
            portYIELD_FROM_ISR();
        }
        return ret == pdTRUE ? 1 : 0;
    } else {
        if (pMsg->type == VM_TIMER) {
            // Never allow timer ticks to clog the queue; only push if queue has <= 1 message
            if (uxQueueMessagesWaiting(s_msg_queue) > 1) {
                return 0;
            }
            return xQueueSend(s_msg_queue, pMsg, 0) == pdTRUE ? 1 : 0;
        } else {
            // Key / Touch / System event: must never be dropped!
            ESP_LOGI(TAG, "Pushing KEY msg type=0x%02X param=0x%04X (queue len=%d)",
                     pMsg->type, pMsg->param, (int)uxQueueMessagesWaiting(s_msg_queue));
            while (xQueueSend(s_msg_queue, pMsg, pdMS_TO_TICKS(50)) != pdTRUE) {
                // Drop oldest message if queue is somehow full
                MsgType dummy;
                xQueueReceive(s_msg_queue, &dummy, 0);
            }
            return 1;
        }
    }
#else
    if (s_host_q_count >= HOST_QUEUE_CAP) return 0;
    s_host_queue[s_host_q_tail] = *pMsg;
    s_host_q_tail = (s_host_q_tail + 1) % HOST_QUEUE_CAP;
    s_host_q_count++;
    return 1;
#endif
}

U8 GuiGetMsg(PtrMsg pMsg) {
    if (!pMsg) return 0;

    // Flush any pending dirty display rectangles before waiting for events
    passport_display_flush();

#ifdef ESP_PLATFORM
    if (!s_msg_queue) return 0;
    if (xQueueReceive(s_msg_queue, pMsg, portMAX_DELAY) == pdTRUE) {
        if (pMsg->type != VM_TIMER) {
            ESP_LOGI(TAG, "Popped msg type=0x%02X param=0x%04X", pMsg->type, pMsg->param);
        }
        return 1;
    }
    return 0;
#else
    if (s_host_q_count <= 0) return 0;
    *pMsg = s_host_queue[s_host_q_head];
    s_host_q_head = (s_host_q_head + 1) % HOST_QUEUE_CAP;
    s_host_q_count--;
    return 1;
#endif
}

void bayeSendKey(int key) {
    MsgType msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = VM_CHAR_FUN;
    msg.param = key;
    GuiPushMsg(&msg);
}

U16 GuiGetKbdState(void) {
    return 0;
}

void GuiSetKbdState(U16 state) {
    (void)state;
}

void GuiSetInputFilter(U8 filter) {
    (void)filter;
}

void GuiSetKbdType(U8 type) {
    (void)type;
}

U8 GuiTranslateMsg(PtrMsg pMsg) {
    (void)pMsg;
    return 1;
}

FAR U8 GuiMsgBox(U8* strMsg, U16 nTimeout) {
    (void)strMsg;
    (void)nTimeout;
    return 0;
}

FAR U8 GuiQueryBox(U8 sel, U8 infoType, U8 *infoData) {
    (void)sel;
    (void)infoType;
    (void)infoData;
    return 1;
}
