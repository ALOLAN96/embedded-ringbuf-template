/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 ALOLAN
 */
/**
 * @file utils_ringbuf.h
 * @brief UTIL 层字节环形缓冲：公开契约独立于 LwRB 数值类型。
 * @note 实例和存储区归调用方。必须先成功 Init；全部成员视为私有。
 *       存储区在使用期间保持有效，不得与读写数据区重叠。
 *       无内部锁；并发访问及统计操作须由调用方同步，详见 README。
 */
#ifndef UTILS_RINGBUF_H
#define UTILS_RINGBUF_H

#include <stddef.h>
#include <stdint.h>
#include "lwrb/lwrb.h"

/** @brief 长度、容量和实际传输量，单位 byte。 */
typedef size_t UTIL_RingBufSize_t;

/** @brief 独立模板状态类型；集成时可在边界映射项目统一状态。 */
typedef enum
{
    UTIL_RINGBUF_STATUS_OK = 0,          /**< 操作成功。 */
    UTIL_RINGBUF_STATUS_INVALID_PARAM,   /**< 空参数或存储区小于 2 byte。 */
    UTIL_RINGBUF_STATUS_SIZE_UNSUPPORTED,/**< 存储尺寸超出固定后端安全索引范围。 */
    UTIL_RINGBUF_STATUS_BACKEND_ERROR,   /**< 底层操作失败。 */
    UTIL_RINGBUF_STATUS_NO_SPACE,        /**< 全量写入空间不足，数据保持不变。 */
    UTIL_RINGBUF_STATUS_NO_DATA          /**< 全量读取数据不足，不消费、不修改输出。 */
} UTIL_RingBufStatus_t;

/** @brief 饱和统计；读取/清零须停止写入或外部同步。 */
typedef struct
{
    UTIL_RingBufSize_t unwrittenBytes;   /**< 有效写请求中未写入量，饱和于 SIZE_MAX。 */
    UTIL_RingBufSize_t incompleteWriteCount; /**< 未完整写入次数，包含满缓冲零写入。 */
} UTIL_RingBufStats_t;

/** @brief 调用方静态或自动分配；禁止复制活跃对象或直接访问内部成员。 */
typedef struct
{
    lwrb_t backend;                     /**< 私有后端；保留布局及编译依赖。 */
    UTIL_RingBufStats_t stats;          /**< 私有统计，由 Write / WriteExact 更新。 */
} UTIL_RingBuf_t;

/* 初始化与存储区绑定 ----------------------------------------------------------*/
UTIL_RingBufStatus_t UTIL_RingBuf_Init(UTIL_RingBuf_t *ring, uint8_t storage[],
                                     UTIL_RingBufSize_t storageSizeBytes);

/* 数据读写与消费 ----------------------------------------------------------*/
UTIL_RingBufSize_t UTIL_RingBuf_Write(UTIL_RingBuf_t *ring, const uint8_t data[],
                                    UTIL_RingBufSize_t length);
UTIL_RingBufSize_t UTIL_RingBuf_Read(UTIL_RingBuf_t *ring, uint8_t data[], UTIL_RingBufSize_t length);
UTIL_RingBufStatus_t UTIL_RingBuf_ReadExact(UTIL_RingBuf_t *ring, uint8_t data[],
                                         UTIL_RingBufSize_t length);
UTIL_RingBufSize_t UTIL_RingBuf_Peek(const UTIL_RingBuf_t *ring, uint8_t data[],
                                   UTIL_RingBufSize_t length);
UTIL_RingBufSize_t UTIL_RingBuf_PeekAt(const UTIL_RingBuf_t *ring, UTIL_RingBufSize_t offsetBytes,
                                     uint8_t data[], UTIL_RingBufSize_t length);
UTIL_RingBufSize_t UTIL_RingBuf_Skip(UTIL_RingBuf_t *ring, UTIL_RingBufSize_t length);
UTIL_RingBufStatus_t UTIL_RingBuf_WriteExact(UTIL_RingBuf_t *ring, const uint8_t data[],
                                          UTIL_RingBufSize_t length);

/* 容量与占用查询 ----------------------------------------------------------*/
UTIL_RingBufSize_t UTIL_RingBuf_GetUsedBytes(const UTIL_RingBuf_t *ring);
UTIL_RingBufSize_t UTIL_RingBuf_GetFreeBytes(const UTIL_RingBuf_t *ring);
UTIL_RingBufSize_t UTIL_RingBuf_GetCapacityBytes(const UTIL_RingBuf_t *ring);

/* 数据清空 ----------------------------------------------------------*/
void UTIL_RingBuf_Clear(UTIL_RingBuf_t *ring);

/* 诊断统计 ----------------------------------------------------------*/
UTIL_RingBufStatus_t UTIL_RingBuf_GetStats(const UTIL_RingBuf_t *ring, UTIL_RingBufStats_t *stats);
void UTIL_RingBuf_ClearStats(UTIL_RingBuf_t *ring);

#endif /* UTILS_RINGBUF_H */
