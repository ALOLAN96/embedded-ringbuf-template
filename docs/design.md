# UTIL_RingBuf 模板设计与实施记录

实现独立、静态分配的字节环形缓冲组件。公开长度类型基于 size_t；
公开 API 不使用 lwrb_sz_t。对象内保留 lwrb_t，因此保留编译和布局依赖，不承诺二进制兼容。
实例及存储区归调用方；内部状态只允许本模块更新。无 HAL、RTOS、堆分配依赖。

接口：Init、Write、Read、ReadExact、Peek、PeekAt、Skip、WriteExact、GetUsedBytes、GetFreeBytes、
GetCapacityBytes、Clear、GetStats、ClearStats。
Write/Read 允许部分传输，返回实际字节数；Clear 仅清空逻辑数据。
统计为 size_t 的 unwrittenBytes 与 incompleteWriteCount，饱和于 SIZE_MAX。
无效参数及零长度不累计统计；重试未写入数据会重复计数，因此不表示真实丢失。

对象必须先成功 Init；失败的参数检查保持原对象。第一次 Init 不读取未初始化状态，
不增加 initialized 标志，不声称能识别任意未初始化或已损坏的对象。
Init/Clear/统计读取或清零必须在读写停止或外部同步下调用。
默认交付外部串行化的 C99 配置；任何并发访问均由集成方同步。
启用 C11 原子路径仍须重新验证目标平台、ISR 安全性以及本封装的统计同步。

类型边界：固定后端 unsigned long。以编译期检查保证其可被 size_t 完整表示；
初始化限制 storageSizeBytes <= ULONG_MAX / 2 + 1，保证后端索引加法不溢出；
传输长度先限制到实例容量再转换，
转换及后端调用集中在 .c。不依赖 unsigned long 与 size_t 恰好同宽。

实施顺序：固定上游快照和许可证；先写边界测试并验证失败；实现接口；
运行真实后端的主机测试、优化构建、32 位编译检查；补中文说明和集成示例。
验收包括容量、回绕、部分读写、无效参数、统计饱和、实例隔离和参考队列比对。
主机测试不替代 MISRA/LDRA 分析、目标编译或硬件并发验证。

## 接口扩展（2026-10-09）

Peek 为 PeekAt 的零偏移入口；PeekAt 按字节偏移查看，不消费。Skip 只推进消费位置。
WriteExact 返回独立状态，NO_SPACE 时保持缓冲数据并统计整个请求未写入；
采用真实后端 WRITE_ALL，转换前以可写空间证明长度可表示。
这些接口遵循外部同步、先成功初始化、禁止存储区重叠的既有契约。

## 全量读取和命名整理（2026-10-16）

新增 ReadExact：NO_DATA 时缓冲和输出不变，零长度 OK，不改变写统计；
内部以已用量校验范围后调用后端 READ_ALL。incompleteWriteCount 统一表达
部分写入或完全未写入的请求次数。字段重命名属于源代码兼容性变化。
并发集成建议见 README；默认推荐单一任务拥有 ring 和 DMA 数据块交接。
