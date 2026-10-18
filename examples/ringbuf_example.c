/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file ringbuf_example.c
 * @brief UTIL 模板串行示例；无 HAL、RTOS 或堆分配。
 */
#include "utils_ringbuf.h"
#include "ringbuf_example.h"
/** @brief 示例独占对象，仅串行访问。 */
static UTIL_RingBuf_t s_rxRing;
/** @brief 示例存储区，64 byte 存储对应 63 byte 可用容量。 */
static uint8_t s_rxStorage[64];

/** @brief 演示固定数据组的全量写入、读取及状态处理。
 * @return 成功为 0，失败为非零。
 * @pre 串行调用，再次调用会重新初始化对象。
 */
int UTIL_RingBuf_Example(void)
{
    const uint8_t input[3] = {0x11U, 0x22U, 0x33U};
    uint8_t output[3];
    int result = 1;
    UTIL_RingBufStatus_t status = UTIL_RingBuf_Init(&s_rxRing, s_rxStorage, sizeof(s_rxStorage));
    if (status == UTIL_RINGBUF_STATUS_OK) {
        UTIL_RingBufStatus_t writeStatus = UTIL_RingBuf_WriteExact(&s_rxRing, input, sizeof(input));
        if (writeStatus == UTIL_RINGBUF_STATUS_OK) {
            UTIL_RingBufStatus_t readStatus = UTIL_RingBuf_ReadExact(&s_rxRing, output, sizeof(output));
            if (readStatus == UTIL_RINGBUF_STATUS_OK) {
                if ((output[0] == input[0]) && (output[1] == input[1]) && (output[2] == input[2])) {
                    result = 0;
                }
            }
        }
    }
    return result;
}
