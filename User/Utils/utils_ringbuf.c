/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/**
 * @file utils_ringbuf.c
 * @brief UTIL 层 LwRB 适配；拥有类型转换、参数校验和饱和统计。
 * @note 固定后端 v3.2.0 的长度类型为 unsigned long；更新后端须复核范围证明。
 */
#include "utils_ringbuf.h"
#include <limits.h>

/* 保证后端返回值可完整表示，不支持 size_t 比后端更窄的平台。 */
#if SIZE_MAX < ULONG_MAX
#error "UTIL_RingBuf requires SIZE_MAX >= ULONG_MAX for the pinned backend"
#endif

/** @brief 后端安全存储尺寸上限，单位 byte。
 * Peek/Skip 内部先累加索引再回绕，最大和为 2 * (size - 1)。
 * 因此限制 size <= ULONG_MAX / 2 + 1，确保该加法不会溢出。
 */
#define UTIL_RINGBUF_MAX_STORAGE_BYTES ((UTIL_RingBufSize_t)(ULONG_MAX / 2UL) + 1U)

/** @brief 饱和加法，避免诊断计数回绕。
 * @param current 原计数。
 * @param increment 本次增量。
 * @retval 饱和于 SIZE_MAX 的和。
 */
static UTIL_RingBufSize_t SaturatingAdd(UTIL_RingBufSize_t current, UTIL_RingBufSize_t increment)
{
    UTIL_RingBufSize_t result = SIZE_MAX;
    if (increment <= (SIZE_MAX - current)) {
        result = current + increment;
    }
    return result;
}

/** @brief 校验范围后绑定外部存储区，成功时清空数据和统计。
 * @param ring 调用方持有的对象；首次初始化不读取旧内容。
 * @param storage 使用期间保持有效且不搬移的外部数组。
 * @param storageSizeBytes 实际数组大小，至少 2 byte。
 * @pre 必须停止实例的全部访问。
 * @retval RingBuf status
 * @note 存储区实际可访问大小必须不少于 storageSizeBytes，且与对象不重叠。
 *       实际可用容量为 storageSizeBytes - 1；存储尺寸上限为 ULONG_MAX / 2 + 1。
 *       首次调用无需预先清零对象；参数或尺寸检查失败保持原对象，初始化失败后禁止读写。
 */
UTIL_RingBufStatus_t UTIL_RingBuf_Init(UTIL_RingBuf_t *ring, uint8_t storage[],
                                     UTIL_RingBufSize_t storageSizeBytes)
{
    UTIL_RingBufStatus_t result = UTIL_RINGBUF_STATUS_INVALID_PARAM;
    if ((ring != NULL) && (storage != NULL) && (storageSizeBytes >= 2U)) {
        if (storageSizeBytes > UTIL_RINGBUF_MAX_STORAGE_BYTES) {
            result = UTIL_RINGBUF_STATUS_SIZE_UNSUPPORTED;
        } else if (lwrb_init(&ring->backend, storage, (lwrb_sz_t)storageSizeBytes) == 0U) {
            result = UTIL_RINGBUF_STATUS_BACKEND_ERROR;
        } else {
            ring->stats.unwrittenBytes = 0U;
            ring->stats.incompleteWriteCount = 0U;
            result = UTIL_RINGBUF_STATUS_OK;
        }
    }
    return result;
}

/** @brief 写入可容纳的前缀，统计有效请求中的未写入量。
 * @param ring 已初始化对象。
 * @param data 至少 length byte 的数组，不与实例或存储区重叠。
 * @param length 请求字节数。
 * @retval 实际写入量，无效参数或零长度返回 0。
 * @note 限制到实例容量后才转换长度；不覆盖旧数据。
 * @note 非零长度的空数据指针也返回 0，不累计统计；零长度允许 data 为空。
 *       未完整写入时累计 unwrittenBytes 与 incompleteWriteCount，计数饱和于 SIZE_MAX。
 *       重试可能重复累计，未写入量不代表实际丢失量。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufSize_t UTIL_RingBuf_Write(UTIL_RingBuf_t *ring, const uint8_t data[],
                                    UTIL_RingBufSize_t length)
{
    UTIL_RingBufSize_t written = 0U;
    if ((ring != NULL) && (data != NULL) && (length > 0U)) {
        UTIL_RingBufSize_t boundedLength = length;
        UTIL_RingBufSize_t capacity = UTIL_RingBuf_GetCapacityBytes(ring);
        if (boundedLength > capacity) {
            boundedLength = capacity;
        }
        written = (UTIL_RingBufSize_t)lwrb_write(&ring->backend, data, (lwrb_sz_t)boundedLength);
        if (written < length) {
            ring->stats.unwrittenBytes = SaturatingAdd(ring->stats.unwrittenBytes, length - written);
            ring->stats.incompleteWriteCount = SaturatingAdd(ring->stats.incompleteWriteCount, 1U);
        }
    }
    return written;
}

/** @brief 读取并消费可用数据。
 * @param ring 已初始化对象。
 * @param data 至少 length byte 的输出数组，不与实例或存储区重叠。
 * @param length 输出数组容量，单位 byte。
 * @retval 实际读取量，无效参数或零长度返回 0。
 * @note 零长度允许 data 为空；非零长度空数据指针返回 0，输出和写入统计不变。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufSize_t UTIL_RingBuf_Read(UTIL_RingBuf_t *ring, uint8_t data[], UTIL_RingBufSize_t length)
{
    UTIL_RingBufSize_t readBytes = 0U;
    if ((ring != NULL) && (data != NULL) && (length > 0U)) {
        UTIL_RingBufSize_t boundedLength = length;
        UTIL_RingBufSize_t capacity = UTIL_RingBuf_GetCapacityBytes(ring);
        if (boundedLength > capacity) {
            boundedLength = capacity;
        }
        readBytes = (UTIL_RingBufSize_t)lwrb_read(&ring->backend, data, (lwrb_sz_t)boundedLength);
    }
    return readBytes;
}

/** @brief 不等待地完整读取；数据不足时不消费、不修改输出。
 * @param ring 已初始化对象。
 * @param data 至少 length byte 的输出数组，不与实例或存储区重叠；零长度可为空。
 * @param length 请求读取字节数。
 * @pre 外部同步整个操作；禁止多个消费者并发修改读位置。
 * @note 以已用量证明长度可表示，后端 READ_ALL 在复制前验证完整数据可用。
 * @retval RingBuf status
 * @note 本操作不修改写入统计；成功后消费 length byte。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufStatus_t UTIL_RingBuf_ReadExact(UTIL_RingBuf_t *ring, uint8_t data[],
                                         UTIL_RingBufSize_t length)
{
    UTIL_RingBufStatus_t result = UTIL_RINGBUF_STATUS_INVALID_PARAM;
    if (ring != NULL) {
        if (length == 0U) {
            result = UTIL_RINGBUF_STATUS_OK;
        } else if (data != NULL) {
            if (length > UTIL_RingBuf_GetUsedBytes(ring)) {
                result = UTIL_RINGBUF_STATUS_NO_DATA;
            } else if (lwrb_read_ex(&ring->backend, data, (lwrb_sz_t)length,
                                    NULL, LWRB_FLAG_READ_ALL) == 0U) {
                result = UTIL_RINGBUF_STATUS_BACKEND_ERROR;
            } else {
                result = UTIL_RINGBUF_STATUS_OK;
            }
        }
    }
    return result;
}

/** @brief 从队首复制数据，不消费。
 * @param ring 已初始化对象。
 * @param data 不与实例及存储区重叠的输出数组。
 * @param length 输出数组容量，单位 byte。
 * @retval 实际复制量；空参数或零长度返回 0。
 * @note 最多复制当前已有数据；不改变消费位置或写入统计。
 *       零长度允许 data 为空，其他输出数组约定与 Read 相同。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufSize_t UTIL_RingBuf_Peek(const UTIL_RingBuf_t *ring, uint8_t data[],
                                   UTIL_RingBufSize_t length)
{
    return UTIL_RingBuf_PeekAt(ring, 0U, data, length);
}

/** @brief 按相对队首偏移复制有效数据，不消费。
 * @param ring 已初始化对象。
 * @param offsetBytes 相对队首的字节偏移。
 * @param data 至少 length byte 的输出数组，不与实例或存储区重叠。
 * @param length 输出数组容量，单位 byte。
 * @retval 实际复制量；偏移越界或无效参数返回 0。
 * @note 校验偏移并限制复制量后才转换后端类型，不计算 offset + length。
 * @note offsetBytes 达到或超过已用量时返回 0；复制量最多为已用量减偏移。
 *       零长度允许 data 为空；不改变消费位置或写入统计。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufSize_t UTIL_RingBuf_PeekAt(const UTIL_RingBuf_t *ring, UTIL_RingBufSize_t offsetBytes,
                                     uint8_t data[], UTIL_RingBufSize_t length)
{
    UTIL_RingBufSize_t copied = 0U;
    if ((ring != NULL) && (data != NULL) && (length > 0U)) {
        UTIL_RingBufSize_t used = UTIL_RingBuf_GetUsedBytes(ring);
        if (offsetBytes < used) {
            UTIL_RingBufSize_t boundedLength = used - offsetBytes;
            if (boundedLength > length) {
                boundedLength = length;
            }
            copied = (UTIL_RingBufSize_t)lwrb_peek(&ring->backend, (lwrb_sz_t)offsetBytes,
                                                 data, (lwrb_sz_t)boundedLength);
        }
    }
    return copied;
}

/** @brief 消费队首数据，不复制。
 * @param ring 已初始化对象。
 * @param length 请求消费量，单位 byte。
 * @retval 实际消费量；空对象或零长度返回 0。
 * @note 最多消费当前已有数据；零长度无操作，不改变写入统计。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufSize_t UTIL_RingBuf_Skip(UTIL_RingBuf_t *ring, UTIL_RingBufSize_t length)
{
    UTIL_RingBufSize_t skipped = 0U;
    if ((ring != NULL) && (length > 0U)) {
        UTIL_RingBufSize_t boundedLength = UTIL_RingBuf_GetUsedBytes(ring);
        if (boundedLength > length) {
            boundedLength = length;
        }
        skipped = (UTIL_RingBufSize_t)lwrb_skip(&ring->backend, (lwrb_sz_t)boundedLength);
    }
    return skipped;
}

/** @brief 不等待地完整写入；空间不足时不修改缓冲数据。
 * @param ring 已初始化对象。
 * @param data 输入数组，不与实例或存储区重叠；零长度时可为空。
 * @param length 请求字节数；成功路径需要有效的整个输入范围。
 * @pre 外部同步整个操作；不提供多写者并发原子性。
 * @note 拒绝超容量请求后再转换长度，后端 WRITE_ALL 负责全量写入。
 * @retval RingBuf status
 * @note NO_SPACE 时累计整个请求的未写入量，并增加一次未完整写入计数。
 *       零长度或无效参数不累计统计；本函数非阻塞。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufStatus_t UTIL_RingBuf_WriteExact(UTIL_RingBuf_t *ring, const uint8_t data[],
                                          UTIL_RingBufSize_t length)
{
    UTIL_RingBufStatus_t result = UTIL_RINGBUF_STATUS_INVALID_PARAM;
    if (ring != NULL) {
        if (length == 0U) {
            result = UTIL_RINGBUF_STATUS_OK;
        } else if (data != NULL) {
            if (length > UTIL_RingBuf_GetFreeBytes(ring)) {
                ring->stats.unwrittenBytes = SaturatingAdd(ring->stats.unwrittenBytes, length);
                ring->stats.incompleteWriteCount = SaturatingAdd(ring->stats.incompleteWriteCount, 1U);
                result = UTIL_RINGBUF_STATUS_NO_SPACE;
            } else if (lwrb_write_ex(&ring->backend, data, (lwrb_sz_t)length,
                                     NULL, LWRB_FLAG_WRITE_ALL) == 0U) {
                result = UTIL_RINGBUF_STATUS_BACKEND_ERROR;
            } else {
                result = UTIL_RINGBUF_STATUS_OK;
            }
        }
    }
    return result;
}

/** @brief 获取已用空间快照。
 * @param ring 已初始化对象，允许为空。
 * @retval 已用字节数，空指针返回 0。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufSize_t UTIL_RingBuf_GetUsedBytes(const UTIL_RingBuf_t *ring)
{
    UTIL_RingBufSize_t result = 0U;
    if (ring != NULL) {
        result = (UTIL_RingBufSize_t)lwrb_get_full(&ring->backend);
    }
    return result;
}

/** @brief 获取空闲空间快照，不构成空间预留。
 * @param ring 已初始化对象，允许为空。
 * @retval 可写字节数，空指针返回 0。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufSize_t UTIL_RingBuf_GetFreeBytes(const UTIL_RingBuf_t *ring)
{
    UTIL_RingBufSize_t result = 0U;
    if (ring != NULL) {
        result = (UTIL_RingBufSize_t)lwrb_get_free(&ring->backend);
    }
    return result;
}

/** @brief 获取实际可用容量。
 * @param ring 已初始化对象，允许为空。
 * @retval 可用字节数，空指针返回 0。
 * @note 返回初始化存储区大小减一，而非当前空闲空间。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufSize_t UTIL_RingBuf_GetCapacityBytes(const UTIL_RingBuf_t *ring)
{
    UTIL_RingBufSize_t result = 0U;
    if (ring != NULL) {
        result = (UTIL_RingBufSize_t)ring->backend.size;
        result -= 1U;
    }
    return result;
}

/** @brief 清空逻辑数据，不擦除存储区或清除统计。
 * @param ring 已初始化对象，空指针无操作。
 * @pre 读写双方均停止，不得与任何实例操作并发。
 * @note 仅清空逻辑数据，外部存储区绑定和统计保持不变；空对象指针无操作。
 * @retval None
 */
void UTIL_RingBuf_Clear(UTIL_RingBuf_t *ring)
{
    if (ring != NULL) {
        lwrb_reset(&ring->backend);
    }
}

/** @brief 在同步条件下复制统计快照。
 * @param ring 已初始化对象。
 * @param stats 输出快照，不得指向对象内部；空参数时输出保持不变。
 * @pre 不得与 Write / WriteExact 或 ClearStats 并发。
 * @retval RingBuf status
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 */
UTIL_RingBufStatus_t UTIL_RingBuf_GetStats(const UTIL_RingBuf_t *ring, UTIL_RingBufStats_t *stats)
{
    UTIL_RingBufStatus_t result = UTIL_RINGBUF_STATUS_INVALID_PARAM;
    if ((ring != NULL) && (stats != NULL)) {
        *stats = ring->stats;
        result = UTIL_RINGBUF_STATUS_OK;
    }
    return result;
}

/** @brief 清零统计，保留数据。
 * @param ring 已初始化对象，空指针无操作。
 * @pre 不得与 Write / WriteExact 或 GetStats 并发。
 * @note 缓冲区数据和外部存储区绑定保持不变；空对象指针无操作。
 * @pre 非空对象须已成功初始化；所有实例访问遵守调用方的串行化或外部同步约定。
 * @retval None
 */
void UTIL_RingBuf_ClearStats(UTIL_RingBuf_t *ring)
{
    if (ring != NULL) {
        ring->stats.unwrittenBytes = 0U;
        ring->stats.incompleteWriteCount = 0U;
    }
}
