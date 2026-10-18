/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file runtime.c
 * @brief 无系统 CRT 的 Windows 主机测试支持；不属于目标固件。
 */
#include <stddef.h>
/** @brief 按字节复制互不重叠的区域。 */
void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *output = destination;
    const unsigned char *input = source;
    size_t index;
    for (index = 0; index < length; ++index) {
        output[index] = input[index];
    }
    return destination;
}
/** @brief 初始化测试存储区域。 */
void *memset(void *destination, int value, size_t length)
{
    unsigned char *output = destination;
    size_t index;
    for (index = 0; index < length; ++index) {
        output[index] = (unsigned char)value;
    }
    return destination;
}
