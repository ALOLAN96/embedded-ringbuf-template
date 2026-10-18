# embedded-ringbuf-template

基于固定版本 LwRB 的字节环形缓冲组件，供嵌入式工程复制使用。
无动态内存、HAL 或 RTOS 依赖；支持调用方静态分配多个独立实例。
适合 UART 字节流缓存、协议解析前查看、完整发送帧入队，以及按组复制的 ADC 数据。
所有 API 非阻塞；默认要求串行调用或调用方同步。

[接口源码](User/Utils/utils_ringbuf.c) · [头文件](User/Utils/utils_ringbuf.h) · [使用示例](examples/ringbuf_example.c) · [验证记录](docs/validation.md) · [MIT 许可证](LICENSE)

## 交付内容

| 路径 | 用途 |
|---|---|
| `User/Utils/utils_ringbuf.h/.c` | 项目拥有的公共接口、类型适配、统计 |
| `User/ThirdParty/lwrb/` | 官方 v3.2.0 核心源码、许可证、来源和 SHA-256 |
| `examples/ringbuf_example.h/.c` | 可编译的串行使用示例 |
| `tests/` | Windows LLVM 主机测试及最小 CRT 支持 |
| `docs/design.md` | 设计决策与实施顺序 |
| `docs/validation.md` | 已完成验证和未验证范围 |

## 接入 Keil / EIDE / 其他 C 工程

1. 复制 `User/Utils` 和 `User/ThirdParty/lwrb`，保留第三方许可证及来源记录。
2. 将 `utils_ringbuf.c`、`User/ThirdParty/lwrb/src/lwrb/lwrb.c` 加入实际构建源文件列表。
3. 添加头文件路径 `User/Utils`、`User/ThirdParty/lwrb/src/include`。
4. 在**全部相关编译单元**统一定义 `LWRB_DISABLE_ATOMIC`。这是默认 C99 配置，
   只承诺串行或由调用方完整同步的使用方式。不能仅对第三方源文件设置这个宏。
5. 以 C99 或兼容模式编译；不要将 `tests/stubs` 加入产品工程头文件路径。
6. 开启接收中断/任务之前完成初始化；根据项目错误策略处理初始化失败。

若使用 CMake，可 `add_subdirectory` 本目录并链接 `util_ringbuf`，上述头文件和宏会随目标传播。
提供的 CMake 入口未在本次环境执行；实际验证使用 `tests/run.py` 直接调用 LLVM。

```c
#include "utils_ringbuf.h"

static UTIL_RingBuf_t rxRing;
static uint8_t rxStorage[256]; /* 可用容量为 255 byte */

/* 在启用任何读写方之前调用；调用方处理初始化失败。 */
UTIL_RingBufStatus_t App_BufferInit(void)
{
    return UTIL_RingBuf_Init(&rxRing, rxStorage, sizeof(rxStorage));
}

/* 仅在 App_BufferInit 成功后调用；外围负责同步。 */
UTIL_RingBufStatus_t App_TransferExample(void)
{
    const uint8_t input[3] = {0x11U, 0x22U, 0x33U};
    uint8_t output[3];
    UTIL_RingBufStatus_t result = UTIL_RingBuf_WriteExact(&rxRing, input, sizeof(input));
    if (result == UTIL_RINGBUF_STATUS_OK) {
        result = UTIL_RingBuf_ReadExact(&rxRing, output, sizeof(output));
        /* 仅当 result 为 OK 时，output 才包含完整读取结果。 */
    }
    return result;
}
```

完整可编译示例见 [examples/](examples/ringbuf_example.c)。此库没有专用板卡、
CubeMX 工程或固件镜像；编译和烧录使用宿主工程现有流程。

## 公共接口及约定

`UTIL_RingBufSize_t` 定义为 `size_t`。所有长度均以 byte 为单位，
不维护消息边界，不覆盖尚未消费的数据。WriteExact 提供单次全量写入，
完整帧的边界仍由调用方维护，外部同步要求不变。

| 接口 | 行为 |
|---|---|
| `Init` | 绑定外部存储区；成功时清空逻辑数据及统计 |
| `Write` | 写入可容纳的前缀，返回实际写入字节数 |
| `Read` | 读取并消费可用前缀，返回实际读取字节数 |
| `ReadExact` | 非阻塞全量读取；不足返回 NO_DATA，不消费、不修改输出 |
| `Peek` | 从队首复制数据，不消费，返回实际复制量 |
| `PeekAt` | 从相对队首的 offsetBytes 偏移复制数据，不消费 |
| `Skip` | 不复制地消费队首数据，返回实际消费量 |
| `WriteExact` | 非阻塞全量写入；空间不足返回 NO_SPACE，缓冲数据不变 |
| `GetUsedBytes` | 已占用字节数快照 |
| `GetFreeBytes` | 可写空间快照，不预留空间 |
| `GetCapacityBytes` | 实际可用容量，等于存储区大小减一 |
| `Clear` | 清空逻辑数据，保留统计和存储区内容 |
| `GetStats` | 向独立输出对象复制统计，返回状态 |
| `ClearStats` | 清零统计，保留缓冲数据 |

- 对象和外部存储区由调用方持有，生命周期覆盖所有操作；二者不可重叠。
- 首次 Init 无需预先清零对象。Init 成功前禁止其他操作；不提供任意无效地址检测。
- 容量、地址、读写指针等后端信息不重复保存。所有公开结构体成员视为私有，
  禁止直接修改、调用底层 API、复制活跃对象或搬移其存储区。
- Init 的空指针、过小存储区和超范围检查失败不修改已有对象。
  若返回 BACKEND_ERROR，须按初始化失败处理，不能继续读写。
- 传入的读写数组至少具有 length byte 的有效范围，不得与对象及其存储区重叠。
- Write/Read/Peek/PeekAt 在长度为零时允许 data 为 NULL，返回 0，不修改数据或统计。
  非零长度但 data 为 NULL 同样返回 0，不计统计；此时属于参数错误。
- 对象指针为 NULL 时，返回数量的接口返回 0，Clear/ClearStats 无操作。
  Init、WriteExact、ReadExact、GetStats 返回 INVALID_PARAM；GetStats 保持输出不变。
- ReadExact 使用状态返回值：有效对象的零长度请求返回 OK，data 可为 NULL；
  非零长度空 data 返回 INVALID_PARAM；数据不足返回 NO_DATA，不消费且输出保持不变。
  所有 ReadExact 调用均不改变写入统计。
- WriteExact 使用状态返回值：有效对象的零长度请求返回 OK，data 可为 NULL；
  非零长度的空 data 返回 INVALID_PARAM；空间不足返回 NO_SPACE，缓冲数据不变。
- Write/Read 等数量接口返回 0 不区分空/满和参数错误；调用方必须遵守参数及初始化契约。
- 本组件没有堆资源，不需要 DeInit。存储区释放前停止全部访问即可。

## 统计含义

`unwrittenBytes` 是有效 Write / WriteExact 请求中 `请求量 - 实际写入量` 的累计值；
`incompleteWriteCount` 是未完整写入的次数，包含缓冲区满时写入 0 byte 的情况。
WriteExact 因空间不足失败时，整次请求计入未写入量，并增加一次未完整写入计数。
Peek / PeekAt / Skip 不改变写入统计。两者使用 `size_t`，饱和于 SIZE_MAX；不会回绕。

重复重试会重复累计未写入量，因此这些计数不表示实际丢失数据。
真实丢弃应由 UART 等资源所有者另行统计。无效参数及零长度请求不累计。
Init 成功清零统计；Clear 保留统计；ClearStats 保留数据。

## 并发和实时边界

**默认配置不提供内部同步。** 最简单的用法是所有操作在同一上下文串行进行。
若任务、ISR 或多个执行上下文共享实例，集成方必须提供与这些上下文匹配的同步，
不得仅因为“一写一读”就假定默认配置安全。普通任务 mutex 不能直接当作 ISR 锁。

不要不加分析地关中断覆盖大块拷贝：Write/Read 的成本随实际字节数增加，
应测量最坏执行时间。Init/Clear 必须停止读写双方；GetStats/ClearStats 须与 Write / WriteExact 同步。
PeekAt 的 offsetBytes 是相对当前队首的字节偏移，偏移达到或超过已用量返回 0；
复制量限制到偏移后的有效数据。Skip 最多消费当前已有数据。
WriteExact 的非空对象零长度请求返回 OK，data 可为空；空对象仍返回 INVALID_PARAM。
非零长度的空 data 返回 INVALID_PARAM。WriteExact / ReadExact 必须在外部同步条件下执行，
全量语义不提供多写者线程安全，也不代表应用层事务或多个 API 调用的原子性。
查询结果是快照；分开调用 GetUsedBytes/GetFreeBytes 不构成原子的一组结果。

若启用 LwRB 的 C11 原子路径，应在全部编译单元一致取消禁用宏，验证编译器、
原子操作实现、目标平台和 ISR 适用性。本封装的统计不是原子变量，仍需独立同步。
本次未验证 C11 并发模式、DMA 缓冲一致性或硬件中断时序。

## 同步接入建议

默认采用单一上下文拥有 ring：UART / ADC 的 ISR 或 DMA 回调仅发布
已完成数据块的描述，所属任务接收后调用 WriteExact，解析或消费也由该任务执行。
发布描述时仍需 ISR 安全队列或明确的原子交接协议；DMA 数据块在任务复制完成前
不得被 DMA 覆盖，可用双缓冲或块池管理归属。描述队列满时也必须有丢弃和诊断策略。

若必须 ISR 直接写 ring、任务直接读 ring，当前默认配置要求由 BSP 提供
覆盖所有相关访问的、适合目标中断优先级的有界同步；ISR 不得等待任务持有的锁。
任务 mutex 仅适用于全部调用者都是任务的情况。关中断覆盖拷贝前必须核算最坏耗时。
Init/Clear 先停止双方；PeekAt 后再 Skip 的组合也需维持同一消费所有权。
本模板不提供平台锁和无锁并发保证，也没有验证硬件并发路径。

## 类型边界与 MISRA

公开长度类型由模板定义为 size_t，没有别名到 lwrb_sz_t。
结构体内部仍包含 lwrb_t，因此保留编译和布局依赖；不是完全不透明对象，
不承诺更换后端后维持 ABI。允许更换后端后重新编译使用方。

固定的 LwRB v3.2.0 使用 unsigned long 表示长度。本实现：

1. 编译期要求 `SIZE_MAX >= ULONG_MAX`，确保后端返回量可完整表示。
2. Init 要求存储区大小在 `2` 至 `ULONG_MAX / 2 + 1` byte 之间，超上限返回 SIZE_UNSUPPORTED。
   底层 Peek/Skip 先做索引加法再回绕；此上限保证 `2 * (size - 1) <= ULONG_MAX`。
   因此限制的不仅是类型转换范围，还包括底层中间运算范围。
3. Read/Write 将请求先限制到已初始化实例的容量，再转成后端类型。
4. 所有后端数值转换集中在 `.c` 中；饱和统计在 size_t 范围内计算。

这套范围证明依赖固定后端类型定义；升级 LwRB 时必须复核，不能只更新版本号。
不支持 size_t 范围小于 unsigned long 的平台，构建时会明确失败。

封装源码采用单一函数末尾返回、数组形式的缓冲参数、独立长度参数、显式状态映射，
并在执行窄化转换前限制范围。**这些是设计约束，不是 MISRA C:2012 合规证明。**
未运行 LDRA；需要按实际版本、规则集、目标类型模型和第三方代码处置策略分析。
不要自动排除第三方库，也不要为消除告警直接改写原始库。
公共结构体引入的上游头文件也属于需要评估的分析范围。

## 许可证与来源

自有封装、示例、测试和文档使用 [MIT License](LICENSE)，Copyright (c) 2026 ALOLAN。
LwRB 核心固定为 v3.2.0，commit `77b2bdbecf60cd80e449f5c61bde72ec195efac4`，
其原版权属于 Tilen MAJERLE。上游源码、许可证和作者信息保持原样；
详见 [第三方声明](THIRD_PARTY_NOTICES.md) 与 [来源及校验值](User/ThirdParty/lwrb/provenance.json)。

## 运行主机验证

需要 Windows、Python 3 和同目录含 `lld.exe` 的 LLVM `clang.exe`；脚本不下载依赖。

```powershell
python tests/run.py --clang "C:/path/to/LLVM/bin/clang.exe"
```

会校验上游 SHA-256，分别在 O0/O2 编译并运行真实后端；另做 32 位编译检查。
输出到本目录忽略的 `build/`，不会修改源码。测试 CRT 支持只用于主机，不随固件链接。

## 命名迁移（2026-10-16）

统计字段 partialWriteCount 已重命名为 incompleteWriteCount，
含义和布局不变，调用方使用旧字段名的源码需更新；不提供旧名宏别名。
ReadExact 与 WriteExact 对称，所有 Exact 接口都返回状态；Read/Write 返回实际字节数。

## 函数注释约定

公开函数的完整中文 Doxygen 文档集中在 `utils_ringbuf.c` 的函数定义前，
头文件保留简洁声明和功能分组；类型、字段、枚举与文件说明仍保留在头文件。
私有函数同样保留实现前的 Doxygen 函数头。调用约定以实现处文档为准，
VS Code 悬停与补全实际呈现由所用语言服务及其索引决定，本次未进行插件显示验证。

返回状态的函数统一使用一行 `@retval RingBuf status`；状态码含义集中在枚举处说明，
函数特有的失败行为保留在 `@brief`、`@param` 或 `@note` 中。返回字节数用简短 `@retval` 描述含义，void 使用 `@retval None`。

## 场景边界

本组件按字节保存数据，不维护 ADC 样本类型、通道组、消息边界或时间戳。
采样长度和读取长度应由使用方保证按样本/采样组对齐。ReadExact / WriteExact 可保证
单次全量传输，不能替代多步事务锁或多生产者队列。
DMA 直接访问内部存储区、零拷贝、覆盖旧数据和平台锁不属于当前公开接口。

## 当前验证

2026-10-18，使用 Windows LLVM Clang 16.0.6：O0/O2 各九组真实后端测试通过，
包含回绕、偏移查看、跳过、全量读写、统计饱和、容量拒绝、空参数、实例隔离，
以及各 10,000 次变长操作与参考 FIFO 比对。32 位编译通过（未执行）。
上游 SHA-256 校验通过。未执行 LDRA/MISRA、CMake 构建或目标硬件并发验证。
