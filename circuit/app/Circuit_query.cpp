#include "../Circuit.h"
// 引入本文件要实现的 Circuit 类完整声明：定义其成员函数前必须看到类定义
#include "../core/ValueCodec.h"
// 引入值编解码器：二进制串与各进制字符串互转、位列表转字符串等都委托给它
#include "../core/Component.h"
// 引入元件相关定义：参与元件类型判断/端口几何计算等
#include "../logic/ComponentLogic.h"
// 引入元件逻辑库：查询端口几何（outputPorts/inputPorts）的唯一来源
#include "../logic/event/EventEngine.h"
// 引入事件引擎：getGateTypeDelay 等接口需要完整类型才能调用
#include <functional>
// 引入 std::function：statistics() 中用递归 lambda 遍历上下文与子电路

// 定义"按 ID 查元件"接口：返回元件 map，不存在返回空 map
QVariantMap Circuit::getComponent(const QString& id) const {
    // 在当前上下文查元件索引
    int idx = indexOfComponent(id);
    // 索引无效说明元件不存在，返回空 map 表示失败
    if (idx < 0) return {};
    // 取出当前上下文对应元件 map 返回
    return currentCtx().components[idx].toMap();
}

// 定义"按 ID 查连线"接口：返回连线 map，不存在返回空 map
QVariantMap Circuit::getWire(const QString& id) const {
    // 在当前上下文查连线索引
    int idx = indexOfWire(id);
    // 索引无效说明连线不存在，返回空 map
    if (idx < 0) return {};
    // 取出当前上下文对应连线 map 返回
    return currentCtx().wires[idx].toMap();
}

// 定义"返回某元件输出端口几何信息"接口
QVariantList Circuit::outputPortsInfo(const QString& id) const {
    // 查元件索引
    int idx = indexOfComponent(id);
    // 元件不存在返回空列表
    if (idx < 0) return {};
    // 委托 ComponentLogic::outputPorts 计算端口几何，保证端口规则唯一来源
    return ComponentLogic::outputPorts(currentCtx().components[idx].toMap());
}

// 定义"返回某元件输入端口几何信息"接口
QVariantList Circuit::inputPortsInfo(const QString& id) const {
    // 查元件索引
    int idx = indexOfComponent(id);
    // 元件不存在返回空列表
    if (idx < 0) return {};
    // 委托 ComponentLogic::inputPorts 计算端口几何
    return ComponentLogic::inputPorts(currentCtx().components[idx].toMap());
}

// 定义"一次性返回所有元件端口信息"接口：QML 连线时批量查询入口
QVariantMap Circuit::allPortsInfo() const {
    // 准备输出 map：key 为元件 id，value 为该元件的 in/out 端口信息
    QVariantMap out;
    // 取当前上下文引用
    const Context& ctx = currentCtx();
    // 遍历当前上下文所有元件
    for (const auto& cv : ctx.components) {
        // 元件转 map
        QVariantMap c = cv.toMap();
        // 取元件 id 作为输出 map 的键
        QString id = c.value("id").toString();
        // 准备该元件的端口信息
        QVariantMap info;
        // 计算并填入输出端口几何
        info["out"] = ComponentLogic::outputPorts(c);
        // 计算并填入输入端口几何
        info["in"] = ComponentLogic::inputPorts(c);
        // 以元件 id 为键写入输出 map
        out[id] = info;
    }
    // 返回所有元件端口信息
    return out;
}

// 定义"查询输入元件显示进制"接口
QString Circuit::getInputBase(const QString& id) const {
    // 查元件索引
    int idx = indexOfComponent(id);
    // 元件不存在则回退默认二进制
    if (idx < 0) return "bin";
    // 取出元件 map
    QVariantMap c = currentCtx().components[idx].toMap();
    // 取 displayBase 字段
    QString b = c.value("displayBase").toString();
    // 字段为空时回退 "bin"
    return b.isEmpty() ? "bin" : b;
}

// 定义"设置输入元件显示进制"接口
void Circuit::setInputBase(const QString& id, const QString& base) {
    // 只读浏览模式禁止修改
    if (m_viewOnly) return;
    // 查元件索引
    int idx = indexOfComponent(id);
    // 元件不存在则返回
    if (idx < 0) return;
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 进制未变则直接返回，避免无意义撤销入栈
    if (c.value("displayBase").toString() == base) return;
    // 压撤销快照，合并键 "base:id"
    pushUndo("base:" + id);
    // 写入新进制
    c["displayBase"] = base;
    // 写回上下文
    ctx.components[idx] = c;
    // 通知数据变化，QML 重新格式化显示
    emit changed();
}

// 定义"格式化值"纯函数接口：把二进制串按指定进制格式化
QString Circuit::formatValue(const QString& binStr, const QString& base, int bw) const {
    // 委托 ValueCodec::format 完成进制转换与位宽处理
    return ValueCodec::format(binStr, base, bw);
}

// 定义"按显示进制读取输入元件值字符串"接口
QString Circuit::getInputAsString(const QString& id) const {
    // 查元件索引
    int idx = indexOfComponent(id);
    // 元件不存在返回空串
    if (idx < 0) return "";
    // 取出元件 map
    QVariantMap c = currentCtx().components[idx].toMap();
    // 取显示进制
    QString base = c.value("displayBase").toString();
    // 进制为空回退 "bin"
    if (base.isEmpty()) base = "bin";
    // 取位宽，下限钳到 1
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    // 位宽上限钳到 64
    if (bw > 64) bw = 64;
    // 取出输入位列表
    QVariantList bits = c.value("inputBits").toList();
    // 位列表转成二进制串（长度按位宽补齐）
    QString binStr = ValueCodec::bitsToString(bits, bw);
    // 再按显示进制格式化返回
    return ValueCodec::format(binStr, base, bw);
}

// 定义"从文本设置输入元件值"接口：解析文本写入 inputBits，返回错误消息（成功为空串）
QString Circuit::setInputFromString(const QString& id, const QString& text) {
    // 只读模式禁止修改，返回提示
    if (m_viewOnly) return "浏览模式不可修改";
    // 查元件索引
    int idx = indexOfComponent(id);
    // 元件不存在返回错误
    if (idx < 0) return "元件不存在";
    // 取当前上下文
    Context& ctx = currentCtx();
    // 取出元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 取类型
    QString t = c.value("type").toString();
    // 只有 input/const 可编辑，其它类型返回错误
    if (t != "input" && t != "const") return "不是可编辑的元件";
    // 取位宽，下限钳到 1
    int bw = c.value("bitWidth").toInt();
    if (bw < 1) bw = 1;
    // 位宽上限钳到 64
    if (bw > 64) bw = 64;
    // 取显示进制
    QString base = c.value("displayBase").toString();
    // 进制为空回退 "bin"
    if (base.isEmpty()) base = "bin";
    // 准备错误消息容器
    QString err;
    // 按进制解析文本为位列表
    QVariantList bits = ValueCodec::parse(text, base, bw, &err);
    // 解析出错则直接返回错误消息
    if (!err.isEmpty()) return err;
    // 解析结果与当前值相同则视为成功但无需改动
    if (c.value("inputBits").toList() == bits) return "";
    // 写入新位列表
    c["inputBits"] = bits;
    // 写回上下文
    ctx.components[idx] = c;

    // 事件引擎运行中：走增量触发路径
    if (m_engineType == "event" && m_eventEngine && m_simulationRunning) {
        // 引擎脏则先重建并跑初始收敛
        if (m_engineDirty) {
            // 重建事件引擎
            rebuildEventEngine();
            // 取重建后的线延迟
            quint64 wd0 = m_eventEngine->wireDelay();
            // 取重建后的门延迟
            quint64 gd0 = m_eventEngine->globalGateDelay();
            // 跑完初始事件收敛
            runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
            // 回写输出到上下文
            applyEngineOutputsToContexts();
        }
        // 位列表转二进制串
        QString binStr = ValueCodec::bitsToString(bits, bw);
        // 取该元件对应的所有引擎 qid
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        // 对每个 qid 触发输入串
        for (const auto& qid : qids)
            m_eventEngine->triggerInputString(qid, binStr);
        // 取线延迟
        quint64 wd = m_eventEngine->wireDelay();
        // 取门延迟
        quint64 gd = m_eventEngine->globalGateDelay();
        // 跑够事件直到稳定
        runEventEngineFully((wd + gd) * 64 + 200, 500000);
        // 回写输出到上下文
        applyEngineOutputsToContexts();
        // 通知 QML 仿真推进
        emit simulationTick();
        // 事件引擎模式但未运行：只标记脏
    } else if (m_engineType == "event") {
        m_engineDirty = true;
        // 非事件引擎：全量求值
    } else {
        evaluateAll();
    }
    // 通知数据变化
    emit changed();
    // 返回空串表示成功
    return "";
}

// 定义"统计各类元件数量"接口：递归遍历根与所有子电路，避免循环引用重复计数
QVariantMap Circuit::statistics() const {
    // 准备结果 map
    QVariantMap result;
    // 按类型统计门电路数量
    QHash<QString, int> typeCount;
    // 各类计数累计量：门、输入、输出、分线器、汇线器、时钟、子实例、文本、常量
    int totalGates = 0, totalInputs = 0, totalOutputs = 0, totalSplits = 0;
    int totalHubs = 0, totalClocks = 0, totalSubInst = 0, totalTexts = 0;
    int totalConsts = 0;

    // 递归计数函数：对单个上下文计数，visited 防止子电路循环引用重复统计
    std::function<void(const Context&, QSet<QString>&)> countCtx =
        [&](const Context& ctx, QSet<QString>& visited) {
            // 遍历该上下文所有元件
            for (const auto& cv : ctx.components) {
                // 元件转 map
                QVariantMap c = cv.toMap();
                // 取类型
                QString t = c.value("type").toString();
                // 文本元件：文本计数 +1，不参与其它统计
                if (t == "text") { totalTexts++; continue; }
                // 输入元件：输入计数 +1
                if (t == "input") { totalInputs++; continue; }
                // 常量元件：常量计数 +1
                if (t == "const") { totalConsts++; continue; }
                // 输出/指示灯：输出计数 +1
                if (t == "output" || t == "led") { totalOutputs++; continue; }
                // 时钟元件：时钟计数 +1
                if (t == "clock") { totalClocks++; continue; }
                // 分线器：分线器计数 +1
                if (t == "splitter") { totalSplits++; continue; }
                // 汇线器：汇线器计数 +1
                if (t == "hub") { totalHubs++; continue; }
                // 子电路实例：实例计数 +1，并递归统计其引用的子电路
                if (t == "sub") {
                    // 子实例计数 +1
                    totalSubInst++;
                    // 取引用的子电路 id
                    QString subId = c.value("subId").toString();
                    // 未访问过才递归，防止循环引用无限递归
                    if (!visited.contains(subId)) {
                        // 标记已访问
                        visited.insert(subId);
                        // 查子电路上下文
                        auto it = m_subContexts.find(subId);
                        // 找到则递归统计
                        if (it != m_subContexts.end()) countCtx(it.value(), visited);
                    }
                    // 子实例分支结束
                    continue;
                }
                // 其余都算逻辑门：门计数 +1，并按类型细分计数
                totalGates++;
                typeCount[t]++;
            }
        };

    // 已访问子电路集合，避免循环引用重复计数
    QSet<QString> visited;
    // 从根上下文开始递归统计
    countCtx(m_root, visited);

    // 写入各类计数结果
    result["totalGates"] = totalGates;
    result["totalInputs"] = totalInputs;
    result["totalConsts"] = totalConsts;
    result["totalOutputs"] = totalOutputs;
    result["totalSplits"] = totalSplits;
    result["totalHubs"] = totalHubs;
    result["totalClocks"] = totalClocks;
    result["totalSubInst"] = totalSubInst;
    result["totalTexts"] = totalTexts;
    // 子电路定义数量
    result["subCount"] = m_subContexts.size();

    // 准备按类型细分的 map
    QVariantMap byType;
    // 遍历类型计数 hash 转成 QVariantMap
    for (auto it = typeCount.begin(); it != typeCount.end(); ++it)
        byType[it.key()] = it.value();
    // 写入按类型细分结果
    result["byType"] = byType;
    // 返回统计结果
    return result;
}