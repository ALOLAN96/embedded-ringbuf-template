/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/** @file test_ringbuf.c
 * @brief 真实后端的主机行为测试；CHECK 宏和直接状态注入仅用于测试。
 */
#include "utils_ringbuf.h"
#include "ringbuf_example.h"
#include <limits.h>
#define CHECK(condition) do { if (!(condition)) { return __LINE__; } } while (0)
#define TEST __declspec(dllexport) int

/** @brief 全量读取失败不消费、不改输出；成功读取跨回绕数据，不影响写统计。 */
TEST test_read_exact(void)
{
    UTIL_RingBuf_t ring;
    uint8_t storage[6];
    uint8_t input[5] = {1, 2, 3, 4, 5};
    uint8_t output[5] = {9, 9, 9, 9, 9};
    UTIL_RingBufStats_t stats;
    CHECK(UTIL_RingBuf_Init(&ring, storage, 6) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_ReadExact(&ring, output, 1) == UTIL_RINGBUF_STATUS_NO_DATA);
    CHECK(output[0] == 9 && output[4] == 9);
    CHECK(UTIL_RingBuf_WriteExact(&ring, input, 4) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_ReadExact(&ring, output, 5) == UTIL_RINGBUF_STATUS_NO_DATA);
    CHECK(UTIL_RingBuf_ReadExact(&ring, output, SIZE_MAX) == UTIL_RINGBUF_STATUS_NO_DATA);
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 4);
    CHECK(output[0] == 9 && output[1] == 9 && output[2] == 9 && output[3] == 9 && output[4] == 9);
    CHECK(UTIL_RingBuf_ReadExact(&ring, output, 3) == UTIL_RINGBUF_STATUS_OK);
    CHECK(output[0] == 1 && output[1] == 2 && output[2] == 3 && output[3] == 9);
    CHECK(UTIL_RingBuf_WriteExact(&ring, input, 4) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_ReadExact(&ring, NULL, 1) == UTIL_RINGBUF_STATUS_INVALID_PARAM);
    CHECK(UTIL_RingBuf_ReadExact(NULL, output, 0) == UTIL_RINGBUF_STATUS_INVALID_PARAM);
    CHECK(UTIL_RingBuf_ReadExact(&ring, NULL, 0) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 5);
    CHECK(UTIL_RingBuf_ReadExact(&ring, output, 5) == UTIL_RINGBUF_STATUS_OK);
    CHECK(output[0] == 4 && output[1] == 1 && output[2] == 2 && output[3] == 3 && output[4] == 4);
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 0);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == 0 && stats.incompleteWriteCount == 0);
    return 0;
}

/** @brief 超安全容量在访问存储前拒绝，已有数据和统计保持不变。 */
TEST test_storage_limit(void)
{
    UTIL_RingBuf_t ring;
    uint8_t storage[3], input[3] = {1, 2, 3}, output[2];
    UTIL_RingBufStats_t stats;
    const size_t firstUnsupported = (size_t)(ULONG_MAX / 2UL) + 2U;
    CHECK(UTIL_RingBuf_Init(&ring, storage, sizeof(storage)) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_Write(&ring, input, 3) == 2);
    /* 故意传入无效尺寸验证前置拒绝；不分配或访问大存储区。 */
    CHECK(UTIL_RingBuf_Init(&ring, storage, firstUnsupported) == UTIL_RINGBUF_STATUS_SIZE_UNSUPPORTED);
    CHECK(UTIL_RingBuf_Init(&ring, storage, (size_t)ULONG_MAX) == UTIL_RINGBUF_STATUS_SIZE_UNSUPPORTED);
    CHECK(UTIL_RingBuf_GetCapacityBytes(&ring) == 2);
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 2);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == 1 && stats.incompleteWriteCount == 1);
    CHECK(UTIL_RingBuf_Read(&ring, output, 2) == 2);
    CHECK(output[0] == 1 && output[1] == 2);
    return 0;
}

/** @brief 验证回绕偏移查看、无消费查看和按实际数量跳过。 */
TEST test_peek_skip(void)
{
    UTIL_RingBuf_t ring;
    uint8_t storage[6], output[6];
    uint8_t input[5] = {1, 2, 3, 4, 5};
    UTIL_RingBufStats_t stats;
    CHECK(UTIL_RingBuf_Init(&ring, storage, sizeof(storage)) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_Write(&ring, input, 5) == 5);
    CHECK(UTIL_RingBuf_Skip(&ring, 3) == 3);
    CHECK(UTIL_RingBuf_Write(&ring, input, 3) == 3);
    CHECK(UTIL_RingBuf_Peek(&ring, output, 6) == 5);
    CHECK(output[0] == 4 && output[1] == 5 && output[2] == 1 && output[4] == 3);
    CHECK(UTIL_RingBuf_PeekAt(&ring, 1, output, 6) == 4);
    CHECK(output[0] == 5 && output[1] == 1 && output[3] == 3);
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 5);
    CHECK(UTIL_RingBuf_PeekAt(&ring, 5, output, 1) == 0);
    CHECK(UTIL_RingBuf_PeekAt(&ring, SIZE_MAX, output, 1) == 0);
    CHECK(UTIL_RingBuf_Peek(NULL, output, 1) == 0);
    CHECK(UTIL_RingBuf_PeekAt(&ring, 0, NULL, 1) == 0);
    CHECK(UTIL_RingBuf_PeekAt(&ring, 0, NULL, 0) == 0);
    CHECK(UTIL_RingBuf_Skip(NULL, 1) == 0);
    CHECK(UTIL_RingBuf_Skip(&ring, 0) == 0);
    CHECK(UTIL_RingBuf_Skip(&ring, SIZE_MAX) == 5);
    CHECK(UTIL_RingBuf_Skip(&ring, 1) == 0);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == 0 && stats.incompleteWriteCount == 0);
    return 0;
}

/** @brief 全量写入失败保持数据，成功可回绕；边界和统计保持一致。 */
TEST test_write_exact(void)
{
    UTIL_RingBuf_t ring;
    uint8_t storage[6], output[5];
    uint8_t input[5] = {1, 2, 3, 4, 5};
    UTIL_RingBufStats_t stats;
    CHECK(UTIL_RingBuf_Init(&ring, storage, 6) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_WriteExact(&ring, input, 4) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_WriteExact(&ring, input, 2) == UTIL_RINGBUF_STATUS_NO_SPACE);
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 4);
    CHECK(UTIL_RingBuf_Read(&ring, output, 3) == 3);
    CHECK(UTIL_RingBuf_WriteExact(&ring, input, 4) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_Read(&ring, output, 5) == 5);
    CHECK(output[0] == 4 && output[1] == 1 && output[4] == 4);
    CHECK(UTIL_RingBuf_WriteExact(&ring, input, 5) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_WriteExact(&ring, input, 1) == UTIL_RINGBUF_STATUS_NO_SPACE);
    CHECK(UTIL_RingBuf_WriteExact(&ring, NULL, 0) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_WriteExact(&ring, NULL, 1) == UTIL_RINGBUF_STATUS_INVALID_PARAM);
    CHECK(UTIL_RingBuf_WriteExact(NULL, input, 0) == UTIL_RINGBUF_STATUS_INVALID_PARAM);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == 3 && stats.incompleteWriteCount == 2);
    /* 超长请求必须在接触输入之前被拒绝，统计增量饱和。 */
    CHECK(UTIL_RingBuf_WriteExact(&ring, input, SIZE_MAX) == UTIL_RINGBUF_STATUS_NO_SPACE);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == SIZE_MAX && stats.incompleteWriteCount == 3);
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 5);
    return 0;
}

/** @brief 验证真实示例的最小集成路径。 */
TEST test_example(void)
{
    return UTIL_RingBuf_Example();
}

/** @brief 验证容量、部分写入、满/空、回绕和顺序。 */
TEST test_fifo(void)
{
    UTIL_RingBuf_t ring;
    uint8_t storage[5];
    uint8_t input[6] = {1, 2, 3, 4, 5, 6};
    uint8_t output[6];
    CHECK(UTIL_RingBuf_Init(&ring, storage, sizeof(storage)) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_GetCapacityBytes(&ring) == 4);
    CHECK(UTIL_RingBuf_GetFreeBytes(&ring) == 4);
    CHECK(UTIL_RingBuf_Read(&ring, output, sizeof(output)) == 0);
    CHECK(UTIL_RingBuf_Write(&ring, input, sizeof(input)) == 4);
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 4);
    CHECK(UTIL_RingBuf_Write(&ring, input, 1) == 0);
    CHECK(UTIL_RingBuf_Read(&ring, output, 3) == 3);
    CHECK(output[0] == 1 && output[1] == 2 && output[2] == 3);
    CHECK(UTIL_RingBuf_Write(&ring, &input[4], 2) == 2);
    CHECK(UTIL_RingBuf_Read(&ring, output, sizeof(output)) == 3);
    CHECK(output[0] == 4 && output[1] == 5 && output[2] == 6);
    CHECK(UTIL_RingBuf_GetFreeBytes(&ring) == 4);
    return 0;
}

/** @brief 无效参数不破坏已有实例；零长度允许空数据指针。 */
TEST test_parameters(void)
{
    UTIL_RingBuf_t ring;
    uint8_t storage[2];
    uint8_t value[1] = {7};
    UTIL_RingBufStats_t stats = {9, 9};
    CHECK(UTIL_RingBuf_Init(NULL, storage, 2) == UTIL_RINGBUF_STATUS_INVALID_PARAM);
    CHECK(UTIL_RingBuf_Init(&ring, NULL, 2) == UTIL_RINGBUF_STATUS_INVALID_PARAM);
    CHECK(UTIL_RingBuf_Init(&ring, storage, 1) == UTIL_RINGBUF_STATUS_INVALID_PARAM);
    CHECK(UTIL_RingBuf_Init(&ring, storage, 2) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_Write(&ring, value, 1) == 1);
    CHECK(UTIL_RingBuf_Init(&ring, storage, 0) == UTIL_RINGBUF_STATUS_INVALID_PARAM);
#if SIZE_MAX > ULONG_MAX
    CHECK(UTIL_RingBuf_Init(&ring, storage, SIZE_MAX) == UTIL_RINGBUF_STATUS_SIZE_UNSUPPORTED);
#endif
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 1);
    CHECK(UTIL_RingBuf_Write(&ring, NULL, 1) == 0);
    CHECK(UTIL_RingBuf_Write(&ring, NULL, 0) == 0);
    CHECK(UTIL_RingBuf_Read(&ring, NULL, 1) == 0);
    CHECK(UTIL_RingBuf_Read(NULL, value, 1) == 0);
    CHECK(UTIL_RingBuf_Write(NULL, value, 1) == 0);
    CHECK(UTIL_RingBuf_GetUsedBytes(NULL) == 0);
    CHECK(UTIL_RingBuf_GetFreeBytes(NULL) == 0);
    CHECK(UTIL_RingBuf_GetCapacityBytes(NULL) == 0);
    CHECK(UTIL_RingBuf_GetStats(NULL, &stats) == UTIL_RINGBUF_STATUS_INVALID_PARAM);
    CHECK(stats.unwrittenBytes == 9 && stats.incompleteWriteCount == 9);
    CHECK(UTIL_RingBuf_GetStats(&ring, NULL) == UTIL_RINGBUF_STATUS_INVALID_PARAM);
    UTIL_RingBuf_Clear(NULL);
    UTIL_RingBuf_ClearStats(NULL);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == 0 && stats.incompleteWriteCount == 0);
    CHECK(UTIL_RingBuf_Read(&ring, value, 1) == 1 && value[0] == 7);
    return 0;
}

/** @brief 统计准确、饱和，Clear 与 ClearStats 的职责互不混淆。 */
TEST test_statistics(void)
{
    UTIL_RingBuf_t ring;
    uint8_t storage[3];
    uint8_t input[4] = {1, 2, 3, 4};
    UTIL_RingBufStats_t stats;
    CHECK(UTIL_RingBuf_Init(&ring, storage, 3) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_Write(&ring, input, 4) == 2);
    CHECK(UTIL_RingBuf_Write(&ring, input, 4) == 0);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == 6 && stats.incompleteWriteCount == 2);
    UTIL_RingBuf_Clear(&ring);
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 0);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == 6);
    CHECK(UTIL_RingBuf_Write(&ring, input, 1) == 1);
    UTIL_RingBuf_ClearStats(&ring);
    CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == 1);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == 0 && stats.incompleteWriteCount == 0);
    /* 测试专用白盒注入，产品调用方禁止直接写入成员。 */
    ring.stats.unwrittenBytes = SIZE_MAX - 1;
    ring.stats.incompleteWriteCount = SIZE_MAX;
    CHECK(UTIL_RingBuf_Write(&ring, input, 4) == 1);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == SIZE_MAX && stats.incompleteWriteCount == SIZE_MAX);
    CHECK(UTIL_RingBuf_Init(&ring, storage, 3) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_GetStats(&ring, &stats) == UTIL_RINGBUF_STATUS_OK);
    CHECK(stats.unwrittenBytes == 0 && UTIL_RingBuf_GetUsedBytes(&ring) == 0);
    return 0;
}

/** @brief 实例隔离和大量变长操作对照简单 FIFO 模型。 */
TEST test_model(void)
{
    UTIL_RingBuf_t ring;
    UTIL_RingBuf_t other;
    uint8_t storage[18], second[3], model[17], input[23], output[23];
    size_t used = 0, step, index, amount, expected, actual;
    uint32_t random = 1234567;
    CHECK(UTIL_RingBuf_Init(&ring, storage, sizeof(storage)) == UTIL_RINGBUF_STATUS_OK);
    CHECK(UTIL_RingBuf_Init(&other, second, sizeof(second)) == UTIL_RINGBUF_STATUS_OK);
    for (step = 0; step < 10000; ++step) {
        random = random * 1664525U + 1013904223U;
        amount = (random >> 8) % 24;
        if ((random & 1U) != 0) {
            for (index = 0; index < amount; ++index) { input[index] = (uint8_t)(step + index); }
            expected = amount < 17 - used ? amount : 17 - used;
            actual = UTIL_RingBuf_Write(&ring, input, amount);
            CHECK(actual == expected);
            for (index = 0; index < actual; ++index) { model[used + index] = input[index]; }
            used += actual;
        } else {
            expected = amount < used ? amount : used;
            actual = UTIL_RingBuf_Read(&ring, output, amount);
            CHECK(actual == expected);
            for (index = 0; index < actual; ++index) { CHECK(output[index] == model[index]); }
            used -= actual;
            for (index = 0; index < used; ++index) { model[index] = model[index + actual]; }
        }
        CHECK(UTIL_RingBuf_GetUsedBytes(&ring) == used);
        CHECK(UTIL_RingBuf_GetFreeBytes(&ring) == 17 - used);
        CHECK(UTIL_RingBuf_GetUsedBytes(&other) == 0);
    }
    return 0;
}
