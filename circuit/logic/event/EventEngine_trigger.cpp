#include "EventEngine.h"
// 引入本文件要实现的 EventEngine 类完整声明：定义其成员函数前必须看到类定义

// 定义"修改输入元件单个 bit 并调度传播"接口
void EventEngine::triggerInputBit(const QString& compId, int bit, bool value) {
    // 按限定 ID 查节点索引
    int idx = idxOf(compId);
    // 节点不存在则返回
    if (idx < 0) return;
    // 取节点引用
    Node& n = m_nodes[idx];
    // 非数据源节点不支持该操作
    if (!n.isInput) return;
    // 取当前输出值；为空则按位宽构造全 0
    QString s = n.outVals.isEmpty() ? QString(n.bitWidth, '0') : n.outVals[0];
    // 长度与位宽不符则重置为全 0，保证按位写入安全
    if (s.length() != n.bitWidth) s = QString(n.bitWidth, '0');
    // 目标位位置
    int pos = bit;
    // 越界则返回
    if (pos < 0 || pos >= s.length()) return;
    // 写入该位新值
    s[pos] = value ? '1' : '0';
    // 写回输出端口值
    if (n.outVals.isEmpty()) n.outVals.append(s); else n.outVals[0] = s;
    // 写回输入端口值（数据源输入端口与输出同值）
    if (n.inVals.isEmpty())  n.inVals.append(s);  else n.inVals[0]  = s;

    // 用块限界，避免 bits 变量与后续代码重名
    {
        // 准备位列表
        QVariantList bits;
        // 预分配容量
        bits.reserve(s.length());
        // 逐位转成 bool
        for (int i = 0; i < s.length(); ++i) bits.append(s[i] == '1');
        // 同步更新节点原始 map 的 inputBits，保持与上下文数据一致
        n.raw["inputBits"] = bits;
    }
    // 记录该节点为脏，供 Circuit 侧增量更新
    markDirty(n.qid);

    // 下游事件时间 = 当前时间 + 线延迟
    quint64 at = m_time + m_opts.wireDelay;
    // 查数据源端口 0 的目标列表
    auto it = m_outDests.constFind(pkey(idx, 0));
    // 有目标则逐个调度 Wire 传播
    if (it != m_outDests.constEnd()) {
        for (const auto& d : it.value())
            scheduleWirePropagation(idx, 0, d.first, d.second, s, at);
    }
}

// 定义"整串设置输入元件值并调度传播"接口
void EventEngine::triggerInputString(const QString& compId, const QString& binStr) {
    // 按限定 ID 查节点索引
    int idx = idxOf(compId);
    // 节点不存在则返回
    if (idx < 0) return;
    // 取节点引用
    Node& n = m_nodes[idx];
    // 非数据源节点不支持该操作
    if (!n.isInput) return;
    // 复制输入串，便于补齐/截断
    QString s = binStr;
    // 长度不足：左侧补 0
    while (s.length() < n.bitWidth) s = "0" + s;
    // 长度超出：只保留低位
    if (s.length() > n.bitWidth) s = s.right(n.bitWidth);
    // 写回输出端口值
    if (n.outVals.isEmpty()) n.outVals.append(s); else n.outVals[0] = s;
    // 写回输入端口值
    if (n.inVals.isEmpty())  n.inVals.append(s);  else n.inVals[0]  = s;

    // 用块限界，避免 bits 变量与后续代码重名
    {
        // 准备位列表
        QVariantList bits;
        // 预分配容量
        bits.reserve(s.length());
        // 逐位转成 bool
        for (int i = 0; i < s.length(); ++i) bits.append(s[i] == '1');
        // 同步更新节点原始 map 的 inputBits
        n.raw["inputBits"] = bits;
    }
    // 记录该节点为脏
    markDirty(n.qid);

    // 下游事件时间 = 当前时间 + 线延迟
    quint64 at = m_time + m_opts.wireDelay;
    // 查数据源端口 0 的目标列表
    auto it = m_outDests.constFind(pkey(idx, 0));
    // 有目标则逐个调度 Wire 传播
    if (it != m_outDests.constEnd()) {
        for (const auto& d : it.value())
            scheduleWirePropagation(idx, 0, d.first, d.second, s, at);
    }
}

// 定义"翻转时钟输出并调度传播"接口
void EventEngine::triggerClockToggle(const QString& compId) {
    // 按限定 ID 查节点索引
    int idx = idxOf(compId);
    // 节点不存在则返回
    if (idx < 0) return;
    // 取节点引用
    Node& n = m_nodes[idx];
    // 非时钟节点不支持该操作
    if (!n.isClock) return;

    // 准备翻转后的值
    QString s;
    // 当前输出值有效则以其为基准
    if (!n.outVals.isEmpty() && n.outVals[0].length() == n.bitWidth)
        s = n.outVals[0];
    // 否则以全 0 为基准
    else
        s = QString(n.bitWidth, '0');

    // 逐位取反
    for (int i = 0; i < s.length(); ++i)
        s[i] = (s[i] == '1') ? '0' : '1';

    // 写回输出端口值
    if (n.outVals.isEmpty()) n.outVals.append(s); else n.outVals[0] = s;
    // 写回输入端口值
    if (n.inVals.isEmpty())  n.inVals.append(s);  else n.inVals[0]  = s;

    // 用块限界
    {
        // 准备输出端口值列表
        QVariantList ops;
        // 填入新值
        ops.append(s);
        // 同步更新节点原始 map 的 outputPorts
        n.raw["outputPorts"] = ops;
    }
    // 记录该节点为脏
    markDirty(n.qid);

    // 下游事件时间 = 当前时间 + 线延迟
    quint64 at = m_time + m_opts.wireDelay;
    // 查时钟端口 0 的目标列表
    auto it = m_outDests.constFind(pkey(idx, 0));
    // 有目标则逐个调度 Wire 传播
    if (it != m_outDests.constEnd()) {
        for (const auto& d : it.value())
            scheduleWirePropagation(idx, 0, d.first, d.second, s, at);
    }
}

// 定义"强制设置某节点某输出端口值并调度传播"接口
void EventEngine::triggerForceOutput(const QString& compId, int port, const QString& value) {
    // 按限定 ID 查节点索引
    int idx = idxOf(compId);
    // 节点不存在则返回
    if (idx < 0) return;
    // 取节点引用
    Node& n = m_nodes[idx];
    // 端口索引越界则返回
    if (port < 0 || port >= n.outVals.size()) return;

    // 时钟元件端口 0：同步更新原始 map 的 outputPorts，保持上下文数据一致
    if (n.isClock && port == 0) {
        QVariantList ops;
        ops.append(value);
        n.raw["outputPorts"] = ops;
    }

    // 值未变则无需传播
    if (n.outVals[port] == value) return;
    // 写入新值
    n.outVals[port] = value;
    // 记录该节点为脏
    markDirty(n.qid);

    // 下游事件时间 = 当前时间 + 线延迟
    quint64 at = m_time + m_opts.wireDelay;
    // 查该输出端口的目标列表
    auto it = m_outDests.constFind(pkey(idx, port));
    // 有目标则逐个调度 Wire 传播
    if (it != m_outDests.constEnd()) {
        for (const auto& d : it.value())
            scheduleWirePropagation(idx, port, d.first, d.second, value, at);
    }
}

// 定义"读取某节点某输出端口值"接口
QString EventEngine::getOutput(const QString& compId, int port) const {
    // 按限定 ID 查节点索引
    int idx = idxOf(compId);
    // 节点不存在返回空串
    if (idx < 0) return QString();
    // 取节点
    const Node& n = m_nodes[idx];
    // 端口索引越界返回空串
    if (port < 0 || port >= n.outVals.size()) return QString();
    // 返回该端口值
    return n.outVals[port];
}

// 定义"读取某节点所有输出端口值"接口
QStringList EventEngine::getOutputs(const QString& compId) const {
    // 按限定 ID 查节点索引
    int idx = idxOf(compId);
    // 节点不存在返回空列表
    if (idx < 0) return QStringList();
    // 取节点
    const Node& n = m_nodes[idx];
    // 准备结果列表
    QStringList out;
    // 预分配容量
    out.reserve(n.outVals.size());
    // 逐个输出端口值追加
    for (const auto& s : n.outVals) out << s;
    // 返回
    return out;
}

// 定义"读取某节点所有输入端口值"接口
QStringList EventEngine::getInputs(const QString& compId) const {
    // 按限定 ID 查节点索引
    int idx = idxOf(compId);
    // 节点不存在返回空列表
    if (idx < 0) return QStringList();
    // 取节点
    const Node& n = m_nodes[idx];
    // 准备结果列表
    QStringList out;
    // 预分配容量
    out.reserve(n.inVals.size());
    // 逐个输入端口值追加
    for (const auto& s : n.inVals) out << s;
    // 返回
    return out;
}