#ifndef JSONCODEC_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define JSONCODEC_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：encode 返回类型、decode 入参类型
#include <QVariantList>
// 引入 QVariantList：实现侧组装 subcircuits 数组时需要（头文件此处为对齐约定而保留）
#include <QHash>
// 引入 QHash：subContexts、subNames、gateTypeDelays、clockStates 都是 QHash
#include <QString>
// 引入 QString：engineType 参数与 subNames 的 key/value 类型
#include "core/CircuitContext.h"
// 引入 CircuitContext 完整定义：encode 以 const 引用接收，DecodedData 以值语义内嵌

/**
 * @brief 存档 JSON 的编解码。
 *
 * 只负责数据层转换，不涉及 Circuit 的成员状态。
 * 解析结果通过 DecodedData 结构返回，由调用者填入自身成员。
 */
// 上方注释点明分层意图：JsonCodec 不认识 Circuit 成员、不碰引擎/定时器，
// 只做"上下文+全局设置 ↔ QVariantMap"的无副作用转换；
// 由 Circuit::toJson/fromJson 负责收集成员、填回成员并触发信号，
// 这样 JsonCodec 可以独立测试，也能被 importFromFile 复用

namespace JsonCodec {
// 用命名空间而非类：encode/decode 是无状态函数，DecodedData 是纯数据返回结构

QVariantMap encode(const CircuitContext& root,
                   const QHash<QString, CircuitContext>& subContexts,
                   const QHash<QString, QString>& subNames,
                   int subCounter,
                   int clockFrequency,
                   int clockTickCount,
                   const QString& engineType,
                   qulonglong globalGateDelay,
                   qulonglong wireDelay,
                   qulonglong simulationSpeed,
                   int simulationSpeedExp,
                   bool autoSpeed,
                   const QHash<QString, quint64>& gateTypeDelays);
// 把所有需要持久化的状态打包成一个 QVariantMap（即 JSON 对象）：
// 参数清单刻意与 Circuit 成员一一对应，但用值传递/const 引用，
// 保证 encode 期间不读取任何全局状态；
// 输出包含 version/root/subcircuits/subCounter/时钟/引擎/速度/门延迟覆盖等键；
// 参数多且为显式列表而非传 Circuit 指针，是为了保持 JsonCodec 与 Circuit 解耦

struct DecodedData {
    // 解码结果容器：字段与 encode 参数一一对应，
    // 默认值即"文件缺少该键时的兜底值"，让 Circuit 侧无需再写默认逻辑

    CircuitContext root;
    // 根上下文：文件损坏或缺失时保持默认空上下文

    QHash<QString, CircuitContext> subContexts;
    // 所有子电路上下文：key 为 S 编号

    QHash<QString, QString> subNames;
    // 子电路显示名：key 为 S 编号

    int subCounter = 0;
    // 子电路编号计数器：恢复后新建子电路不会复用旧编号

    int clockFrequency = 2;
    // 时钟频率（Hz）：缺省 2Hz，与 Circuit 默认值一致

    int clockTickCount = 0;
    // 时钟已跳变计数：保持显示连续性

    QString engineType = "event";
    // 引擎类型：缺省事件驱动，decode 中还会校验只允许 event/iter

    qulonglong globalGateDelay = 1;
    // 全局门延迟：下限 1

    qulonglong wireDelay = 1;
    // 线延迟：下限 1

    qulonglong simulationSpeed = 1000;
    // 仿真速度：缺省 1000 单位/帧

    int simulationSpeedExp = 3;
    // 速度指数：0..12，缺省 3 对应 10^3=1000

    bool autoSpeed = true;
    // 是否自动速度：缺省开启

    QHash<QString, quint64> gateTypeDelays;
    // 按门类型的延迟覆盖：key 为类型名，value 为延迟

    QHash<QString, bool> clockStates;
    // 各时钟元件的当前输出状态：decode 时由 fixCtx 顺带收集并初始化，
    // 因为文件本身不存储运行中的时钟相位（时钟在加载时统一从 0 开始）
};

DecodedData decode(const QVariantMap& data);
// 把 encode 产生的 map 反解析成 DecodedData：
//   - 读取 root/subcircuits 的元件、连线、计数器
//   - 读取全局设置并做范围钳位（频率 1..1000、速度指数 0..12、延迟 ≥1）
//   - 对每个上下文调用 fixCtx：补全 displayBase/name/content，
//     规范化 inputBits 长度与 outputSplits，收集 clockStates
// 返回纯数据，不触碰 Circuit 成员、不发信号

} // namespace JsonCodec
// 结束 JsonCodec 命名空间

#endif // JSONCODEC_H
// 结束头文件保护