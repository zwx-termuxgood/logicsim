#include "JsonCodec.h"
// 引入 JsonCodec 命名空间声明：实现 encode/decode 前必须看到函数原型与 DecodedData 结构
#include <QtGlobal>
// 引入 Qt 全局定义：qBound 等工具函数在此声明

namespace JsonCodec {
// 打开 JsonCodec 命名空间，与头文件声明保持一致

// 定义"编码所有持久化状态"接口：打包成一个 QVariantMap（JSON 对象）
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
                   const QHash<QString, quint64>& gateTypeDelays)
{
    // 准备结果 map
    QVariantMap result;
    // 写入存档格式版本号，供将来兼容性判断
    result["version"] = 11;
    // 写入根上下文：元件、连线、两个计数器
    result["root"] = QVariantMap{
        {"components", root.components},
        {"wires", root.wires},
        {"compCounter", root.compCounter},
        {"wireCounter", root.wireCounter}
    };
    // 准备子电路数组
    QVariantList subs;
    // 遍历所有子电路上下文
    for (auto it = subContexts.begin(); it != subContexts.end(); ++it) {
        // 准备单个子电路 map
        QVariantMap s;
        // 写入子电路 id
        s["id"] = it.key();
        // 写入显示名（缺失回退空串）
        s["name"] = subNames.value(it.key(), "");
        // 写入元件列表
        s["components"] = it.value().components;
        // 写入连线列表
        s["wires"] = it.value().wires;
        // 写入元件计数器
        s["compCounter"] = it.value().compCounter;
        // 写入连线计数器
        s["wireCounter"] = it.value().wireCounter;
        // 追加到子电路数组
        subs.append(s);
    }
    // 写入子电路数组
    result["subcircuits"] = subs;
    // 写入子电路编号计数器
    result["subCounter"] = subCounter;
    // 写入时钟频率
    result["clockFrequency"] = clockFrequency;
    // 写入时钟跳变计数
    result["clockTickCount"] = clockTickCount;
    // 写入引擎类型
    result["engineType"] = engineType;
    // 写入全局门延迟（显式指定 qulonglong 类型，避免 QVariant 精度/类型退化）
    result["globalGateDelay"] = QVariant::fromValue<qulonglong>(globalGateDelay);
    // 写入线延迟
    result["wireDelay"] = QVariant::fromValue<qulonglong>(wireDelay);
    // 写入仿真速度
    result["simulationSpeed"] = QVariant::fromValue<qulonglong>(simulationSpeed);
    // 写入速度指数
    result["simulationSpeedExp"] = simulationSpeedExp;
    // 写入自动速度标志
    result["autoSpeed"] = autoSpeed;
    // 准备按门类型的延迟覆盖 map
    QVariantMap gateDelays;
    // 逐个转换写入
    for (auto it = gateTypeDelays.begin(); it != gateTypeDelays.end(); ++it) {
        gateDelays[it.key()] = QVariant::fromValue<qulonglong>(it.value());
    }
    // 写入门类型延迟覆盖
    result["gateTypeDelays"] = gateDelays;
    // 返回编码结果
    return result;
}

// 定义"修复单个上下文"辅助函数：补全缺失字段、规范化位列表、收集时钟状态
static void fixCtx(CircuitContext& ctx, QHash<QString, bool>& clockStates) {
    // 逐个元件处理
    for (int i = 0; i < ctx.components.size(); ++i) {
        // 元件转 map
        QVariantMap c = ctx.components[i].toMap();
        // 缺失 displayBase 则补默认 "bin"
        if (!c.contains("displayBase")) c["displayBase"] = "bin";
        // 缺失 name 则补空串
        if (!c.contains("name")) c["name"] = "";
        // 文本元件缺失 content 则补默认文本
        if (c.value("type").toString() == "text" && !c.contains("content"))
            c["content"] = "文本";
        // 取位宽，下限钳到 1
        int bw = c.value("bitWidth").toInt();
        if (bw < 1) bw = 1;
        // 取位列表
        QVariantList bits = c.value("inputBits").toList();
        // 长度不足：在高位侧补 0（prepend 保持低位对齐）
        while (bits.size() < bw) bits.prepend(false);
        // 长度超出：从高位侧裁掉
        while (bits.size() > bw) bits.removeFirst();
        // 写回规范化后的位列表
        c["inputBits"] = bits;
        // 取类型
        QString t = c.value("type").toString();
        // 分线器/汇线器缺失分段方案则重置为每段 1 位
        if ((t == "splitter" || t == "hub") && !c.contains("outputSplits")) {
            // 准备分段列表
            QVariantList splits;
            // 每段 1 位，共 bw 段
            for (int j = 0; j < bw; ++j) splits.append(1);
            // 写入分段
            c["outputSplits"] = splits;
        }
        // 时钟元件登记初始状态 false
        if (t == "clock") clockStates[c.value("id").toString()] = false;
        // 写回修复后的元件
        ctx.components[i] = c;
    }
}

// 定义"解码存档数据"接口：把 map 反解析成 DecodedData，并做范围钳位与字段修复
DecodedData decode(const QVariantMap& data) {
    // 准备结果，字段默认值即缺失键时的兜底
    DecodedData out;

    // 存在 root 键则解析根上下文
    if (data.contains("root")) {
        // 取根 map
        QVariantMap r = data.value("root").toMap();
        // 恢复元件列表
        out.root.components = r.value("components").toList();
        // 恢复连线列表
        out.root.wires = r.value("wires").toList();
        // 恢复元件计数器
        out.root.compCounter = r.value("compCounter").toInt();
        // 恢复连线计数器
        out.root.wireCounter = r.value("wireCounter").toInt();
    }

    // 取子电路数组
    QVariantList subs = data.value("subcircuits").toList();
    // 逐个解析子电路
    for (auto& sv : subs) {
        // 子电路转 map
        QVariantMap s = sv.toMap();
        // 取子电路 id
        QString id = s.value("id").toString();
        // 准备子电路上下文
        CircuitContext c;
        // 恢复元件列表
        c.components = s.value("components").toList();
        // 恢复连线列表
        c.wires = s.value("wires").toList();
        // 恢复元件计数器
        c.compCounter = s.value("compCounter").toInt();
        // 恢复连线计数器
        c.wireCounter = s.value("wireCounter").toInt();
        // 登记到子电路表
        out.subContexts[id] = c;
        // 恢复显示名
        out.subNames[id] = s.value("name").toString();
    }

    // 恢复子电路编号计数器
    out.subCounter = data.value("subCounter").toInt();

    // 时钟频率钳位到 [1,1000]，并处理非法值
    out.clockFrequency = qBound(1, data.value("clockFrequency").toInt(), 1000);
    // 值非法（如 0 或负）时回退默认 2Hz
    if (out.clockFrequency < 1) out.clockFrequency = 2;

    // 恢复时钟跳变计数
    out.clockTickCount = data.value("clockTickCount").toInt();
    // 负数视为非法，归零
    if (out.clockTickCount < 0) out.clockTickCount = 0;

    // 恢复引擎类型，缺省 event
    out.engineType = data.value("engineType", "event").toString();
    // 只接受 iter/event，其它值回退 event
    if (out.engineType != "iter" && out.engineType != "event")
        out.engineType = "event";

    // 恢复全局门延迟，下限 1
    qulonglong gd = data.value("globalGateDelay").toULongLong();
    if (gd < 1) gd = 1;
    // 写回
    out.globalGateDelay = gd;

    // 恢复线延迟，下限 1
    qulonglong wd = data.value("wireDelay").toULongLong();
    if (wd < 1) wd = 1;
    // 写回
    out.wireDelay = wd;

    // 取门类型延迟覆盖 map
    QVariantMap gmap = data.value("gateTypeDelays").toMap();
    // 逐个转换恢复
    for (auto it = gmap.begin(); it != gmap.end(); ++it) {
        qulonglong v = it.value().toULongLong();
        out.gateTypeDelays[it.key()] = v;
    }

    // 恢复仿真速度
    out.simulationSpeed = data.value("simulationSpeed").toULongLong();
    // 非法（0 或缺失）时回退默认 1000
    if (out.simulationSpeed < 1) out.simulationSpeed = 1000;

    // 恢复速度指数
    out.simulationSpeedExp = data.value("simulationSpeedExp").toInt();
    // 下限钳到 0
    if (out.simulationSpeedExp < 0) out.simulationSpeedExp = 0;
    // 上限钳到 12
    if (out.simulationSpeedExp > 12) out.simulationSpeedExp = 12;

    // 恢复自动速度标志，缺省 true
    out.autoSpeed = data.value("autoSpeed", true).toBool();

    // 修复根上下文并收集时钟状态
    fixCtx(out.root, out.clockStates);
    // 逐个修复子电路上下文并收集时钟状态
    for (auto it = out.subContexts.begin(); it != out.subContexts.end(); ++it)
        fixCtx(it.value(), out.clockStates);

    // 返回解码结果
    return out;
}

} // namespace JsonCodec
// 关闭 JsonCodec 命名空间