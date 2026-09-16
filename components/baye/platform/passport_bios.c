#include <stdio.h>
#include <stdlib.h>
#include "baye/compa.h"
#include "baye/comm.h"
#include "baye/consdef.h"
#include "baye/order.h"

U8 *_VS_PTR = NULL;         /* 虚拟屏幕缓冲 */
U8 *_BVS_PTR = NULL;        /* 二级虚拟屏幕缓冲 */
U8 *_SHARE_MEM = NULL;      /* 共享临时内存 */
U8 *_FIGHTERS_IDX = NULL;   /* 出征武将队列索引(30个字节) */
U8 *_FIGHTERS = NULL;       /* 出征武将队列(30*10=300个字节) */
U8 *_ORDERQUEUE = NULL;     /* 命令队列(12*100=1200个字节) */

void DataBankSwitch(U8 logicStartBank, U8 bankNumber, U16 physicalStartBank) {
    (void)logicStartBank;
    (void)bankNumber;
    (void)physicalStartBank;
}

void GetDataBankNumber(U8 logicStartBank, U16 *physicalBankNumber) {
    (void)logicStartBank;
    if (physicalBankNumber) {
        *physicalBankNumber = 0;
    }
}

static void _shm_init(void) {
    if (_VS_PTR == NULL) {
        _VS_PTR = (U8 *)calloc(1, MAX_SCR_BUF_LEN);
        _BVS_PTR = (U8 *)calloc(1, MAX_SCR_BUF_LEN);
        _SHARE_MEM = (U8 *)calloc(1, 20240);
        _FIGHTERS_IDX = (U8 *)calloc(1, 30);
        _FIGHTERS = (U8 *)calloc(1, 300);
        _ORDERQUEUE = (U8 *)calloc(1, sizeof(OrderType) * ORDER_MAX);
    }
}

void FlashInit(void) {
    _shm_init();
}

void ResetFlash(void) {
}
