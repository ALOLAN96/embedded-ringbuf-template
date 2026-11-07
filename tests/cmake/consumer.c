/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file consumer.c @brief CMake 头文件、宏传播及最终链接验证。 */
#include "utils_ringbuf.h"
#ifndef LWRB_DISABLE_ATOMIC
#error "consumer must inherit LWRB_DISABLE_ATOMIC"
#endif
/** @brief 使用消费方分配的对象验证字节写入和读出。 */
int main(void)
{
    UTIL_RingBuf_t ring;
    uint8_t storage[8], input[2] = {1U, 2U}, output[2];
    if (UTIL_RingBuf_Init(&ring, storage, sizeof(storage)) != UTIL_RINGBUF_STATUS_OK) { return 1; }
    if (UTIL_RingBuf_WriteExact(&ring, input, sizeof(input)) != UTIL_RINGBUF_STATUS_OK) { return 2; }
    if (UTIL_RingBuf_ReadExact(&ring, output, sizeof(output)) != UTIL_RINGBUF_STATUS_OK) { return 3; }
    return output[0] == 1U && output[1] == 2U ? 0 : 4;
}
