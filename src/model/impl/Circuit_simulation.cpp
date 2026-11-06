// ============================================================================
// Circuit_simulation.cpp（逐行注释版）
// 原始代码：Circuit_simulation.cpp（一字未改，本文件只在代码行上方插入注释行）
// 职责：时钟（clock 元件）与事件驱动仿真的运行控制——
//   tick / startClock / stopClock / setClockFrequency / singleStep / toggleClock /
//   resetAllInputs / 门类型延迟三件套 / startSimulation / pauseSimulation /
//   resetSimulation / simulateStep / onSimTimerTick
// 约定：每行代码上方都有一条中文注释；★ 段是源文件已有的历史缺陷修复点，注释额外说明原写法错在哪。
// ============================================================================

// 包含 Circuit 类声明：本文件所有成员函数定义、m_engineType 等成员访问都依赖它
#include "model/Circuit.h"
// 事件引擎完整定义：本文件大量调用 run/step/reset/triggerForceOutput 等接口，仅前置声明不够
#include "engine/event/EventEngine.h"

// 定时器每帧推进时允许处理的事件数上限（内部链接常量）：防止一帧处理过多事件把 UI 线程卡住
// ★ 修复：从 200000 降到 30000。
//   对于振荡电路（如 not 门自环、奇数反相器环），每 2 个仿真时间单位就会产生 2 个事件，
//   在默认仿真速度（1000 单位/帧）下，单帧就会产生约 1000 个事件；如果电路更大或速度更高，
//   单帧事件数会远超 200000，处理耗时 20~100ms，UI 帧率跌到 10~20fps，视觉上表现为
//   "卡卡的闪烁"而不是流畅的闪烁。降到 30000 后，单帧处理耗时约 3~15ms，配合 16ms
//   帧间隔 UI 保持 50fps 以上；振荡器下多余的帧内事件会自然堆积到下一帧，视觉上是
//   "每帧稳定推进若干个翻转"，用户能真正看到闪烁。对非振荡电路完全没有影响
//   —— 正常电路每帧的事件数远小于 30000，通常几十到几百个事件就跑完并空闲。
static const quint64 kSimRunBudget = 30000;

// 时钟定时器回调：把每个 clock 元件翻转一次，并让电路重新求值
// （private slots，见 Circuit.h；由 m_clockTimer 的 timeout 连接过来）
void Circuit::tick() {
    // 事件引擎模式下，仿真没在跑时时钟没有意义：先判断这个组合
    if (m_engineType == "event" && !m_simulationRunning) {
        // 若时钟仍在跑就顺手停掉，避免定时器空转继续翻转数据
        if (m_clockRunning) stopClock();
        // 直接返回，不做任何翻转
        return;
        // 结束当前代码块
    }
    // 时钟跳变计数累加，用于界面显示/调试
    m_clockTickCount++;
    // 遍历所有时钟元件的状态表（key=元件ID，value=当前电平）
    for (auto it = m_clockStates.begin(); it != m_clockStates.end(); ++it)
        // 每个时钟电平取反：这就是"时钟"的物理语义——高低电平交替
        it.value() = !it.value();

    // 局部 lambda：把一个上下文里所有 clock 元件的 outputPorts 按状态表刷新
    // [&] 按引用捕获，直接读写 m_clockStates
    auto updateClockOutputs = [&](Context& ctx) {
        // 按下标遍历元件，便于后面写回
        for (int i = 0; i < ctx.components.size(); ++i) {
            // 取出元件 map 的副本
            QVariantMap c = ctx.components[i].toMap();
            // 只处理 clock 类型
            if (c.value("type").toString() != "clock") continue;
            // 查该时钟的当前电平，查不到默认 false
            bool v = m_clockStates.value(c.value("id").toString(), false);
            // 读出位宽：时钟输出是 bw 位全 1 或全 0 的串
            int bw = c.value("bitWidth").toInt();
            // 位宽至少 1，防止 QString(0,'1') 得到空串
            if (bw < 1) bw = 1;
            // 准备输出端口数组
            QVariantList out;
            // 按电平构造全 1/全 0 的 bw 位串，作为唯一一个输出端口
            out.append(v ? QString(bw, '1') : QString(bw, '0'));
            // 写回元件 map
            c["outputPorts"] = out;
            // 写回上下文列表（触发隐式共享分裂，不影响其它持有者）
            ctx.components[i] = c;
            // 结束当前代码块
        }
        // 结束 lambda 定义并补分号
    };

    // 当前是否"把某个子电路当作根来编辑"：决定事件引擎结果写回哪一层
    bool editingSubAsRoot = !m_currentCtxId.isEmpty() && !m_viewOnly;

    // 事件引擎分支：时钟翻转要直接喂给引擎，而不是只改数据
    if (m_engineType == "event" && m_eventEngine) {
        // 局部 lambda：刷一层上下文里所有时钟的输出，并同步触发引擎的强制输出
        // ctxId 用于把本地 ID 翻译成带实例前缀的限定 ID
        auto applyToCtx = [&](Context& ctx, const QString& ctxId) {
            // 遍历该上下文的元件
            for (int i = 0; i < ctx.components.size(); ++i) {
                // 取出元件 map 副本
                QVariantMap c = ctx.components[i].toMap();
                // 只处理 clock
                if (c.value("type").toString() != "clock") continue;
                // 该时钟当前电平，查不到按低电平处理
                bool v = m_clockStates.value(c.value("id").toString(), false);
                // 位宽
                int bw = c.value("bitWidth").toInt();
                // 位宽兜底，保证至少输出 1 位
                if (bw < 1) bw = 1;
                // 构造全 1 / 全 0 的 bw 位串
                QString valStr = v ? QString(bw, '1') : QString(bw, '0');
                // 输出端口数组
                QVariantList out;
                // 时钟只有一个输出端口
                out.append(valStr);
                // 写回 map
                c["outputPorts"] = out;
                // 写回上下文
                ctx.components[i] = c;
                // 把本地元件 ID 翻译成引擎限定 ID（同一子电路可能被多个实例引用）
                QStringList qids = engineQidsFor(ctxId, c.value("id").toString());
                // 逐个实例处理
                for (const auto& qid : qids)
                    // 强制写该节点第 0 个输出口的值并向下游传播——时钟驱动仿真的关键入口
                    m_eventEngine->triggerForceOutput(qid, 0, valStr);
                // 结束当前代码块
            }
            // 结束 lambda 定义并补分号
        };
        // 编辑子电路作为根时，引擎顶层就是该子电路
        if (editingSubAsRoot) {
            // 只刷当前上下文；ctxId 传空表示"无实例前缀"
            applyToCtx(currentCtx(), QString());
            // 否则引擎以根为顶层，走下面的分支
        } else {
            // 根上下文的时钟无前缀
            applyToCtx(m_root, QString());
            // 遍历所有子电路
            for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
                // 子电路里的时钟：用其上下文 ID 作为前缀查限定 ID
                applyToCtx(it.value(), it.key());
            // 结束当前代码块
        }
        // 读线延迟
        quint64 wd = m_eventEngine->wireDelay();
        // 读全局门延迟
        quint64 gd = m_eventEngine->globalGateDelay();
        // 让信号传播到稳定：单次翻转深度有限，(线延迟+门延迟)×8+20 足够
        runEventEngineFully((wd + gd) * 8 + 20, 200000);
        // 把引擎结果写回元件的显示字段
        applyEngineOutputsToContexts();
        // 否则走迭代引擎分支
    } else {
        // 迭代引擎没有事件队列，改完数据全量重算即可
        // 刷新根上下文的时钟输出
        updateClockOutputs(m_root);
        // 遍历子电路
        for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
            // 刷新子电路里的时钟输出
            updateClockOutputs(it.value());
        // 全量迭代求值，让变化传播到所有元件
        evaluateAll();
        // 结束当前代码块
    }
    // 通知通用数据变化，画布重绘
    emit changed();
    // 通知时钟状态变化（计数已变）
    emit clockChanged();
    // 结束当前代码块
}

// 启动时钟定时器
void Circuit::startClock() {
    // 定时器指针为空（构造未完成）时直接返回
    if (!m_clockTimer) return;
    // 事件引擎模式下必须先启动仿真，否则时钟翻转无人消费，这里直接拒绝
    if (m_engineType == "event" && !m_simulationRunning) return;
    // 置运行标志
    m_clockRunning = true;
    // 由频率(Hz)换算毫秒间隔：1000/hz；两层 qMax 防止频率为 0 时除零、防止间隔为 0
    int interval = qMax(1, 1000 / qMax(1, m_clockFrequency));
    // 启动/重启定时器（已在运行时会重设间隔）
    m_clockTimer->start(interval);
    // 通知界面切换为"运行中"状态
    emit clockChanged();
    // 结束当前代码块
}

// 停止时钟定时器
void Circuit::stopClock() {
    // 空指针保护
    if (!m_clockTimer) return;
    // 清运行标志
    m_clockRunning = false;
    // 停止定时器，不再产生 tick
    m_clockTimer->stop();
    // 通知界面
    emit clockChanged();
    // 结束当前代码块
}

// 设置时钟频率（Hz）
void Circuit::setClockFrequency(int hz) {
    // 钳位到 1..1000：过快 UI 跟不上，过慢/为 0 会导致间隔计算异常
    m_clockFrequency = qBound(1, hz, 1000);
    // 只有时钟正在跑时才需要立刻生效
    if (m_clockRunning) {
        // 重新换算毫秒间隔（此时频率已保证 ≥1）
        int interval = qMax(1, 1000 / m_clockFrequency);
        // 重启定时器应用新间隔
        m_clockTimer->start(interval);
        // 结束当前代码块
    }
    // 频率属于用户偏好，持久化到 QSettings
    saveSettings();
    // 通知界面刷新
    emit clockChanged();
    // 结束当前代码块
}

// ★ 修复：rebuild 后必须先跑完初始事件再 apply，否则用初始 0 覆盖 ctx 输出
// 原因：build() 之后事件队列里只有初始重算事件，直接 apply 会把引擎里的默认 0 写回上下文，
//       覆盖掉界面上原本正确的值；所以下面所有 rebuild 之后都先 runEventEngineFully 再 apply。
// 单步：按当前引擎类型推进一步
void Circuit::singleStep() {
    // 事件引擎模式
    if (m_engineType == "event") {
        // 拓扑已过期：结构改过但还没重建
        if (m_engineDirty) {
            // 重建引擎拓扑（同时清 m_engineDirty、置 m_needFullApply）
            rebuildEventEngine();
            // 读线延迟
            quint64 wd0 = m_eventEngine->wireDelay();
            // 读门延迟
            quint64 gd0 = m_eventEngine->globalGateDelay();
            // 先把初始事件跑完（深度估计 ×64+200，预算 50 万事件）
            runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
            // 再回写，保证显示的是收敛后的值而不是默认 0
            applyEngineOutputsToContexts();
            // 结束当前代码块
        }
        // 引擎存在才继续（rebuild 失败时可能为空）
        if (m_eventEngine) {
            // 推进一个事件（引擎内部最小的调度单位）
            m_eventEngine->step();
            // 回写结果
            applyEngineOutputsToContexts();
            // 通知仿真前进了一步
            emit simulationTick();
            // 通知通用数据变化
            emit changed();
            // 结束当前代码块
        }
        // 事件模式处理完毕，不再走下面的 tick()
        return;
        // 结束当前代码块
    }
    // 迭代引擎模式没有"事件"，把单步定义为"时钟 tick 一次"
    tick();
    // 结束当前代码块
}

// 手动翻转某一个 clock 元件（用户点按时钟）
void Circuit::toggleClock(const QString& id) {
    // 浏览模式禁止修改
    if (m_viewOnly) return;
    // 在当前上下文里查元件下标
    int idx = indexOfComponent(id);
    // 不存在就返回
    if (idx < 0) return;
    // 取当前上下文引用
    Context& ctx = currentCtx();
    // 取出该元件 map
    QVariantMap c = ctx.components[idx].toMap();
    // 只有 clock 才能翻转
    if (c.value("type").toString() != "clock") return;
    // 取反当前状态（缺省视为 false）
    bool v = !m_clockStates.value(id, false);
    // 写回状态表
    m_clockStates[id] = v;
    // 位宽
    int bw = c.value("bitWidth").toInt();
    // 位宽兜底
    if (bw < 1) bw = 1;
    // 构造全 1/全 0 串
    QString valStr = v ? QString(bw, '1') : QString(bw, '0');
    // 输出端口数组
    QVariantList out;
    // 单端口
    out.append(valStr);
    // 写回 map
    c["outputPorts"] = out;
    // 写回上下文（先改数据，保证界面立刻响应）
    ctx.components[idx] = c;
    // 事件引擎模式
    if (m_engineType == "event" && m_eventEngine) {
        // 仿真没跑：只改数据、不碰引擎，发个信号就结束
        if (!m_simulationRunning) { emit changed(); return; }
        // 翻译成引擎限定 ID（可能多个实例）
        QStringList qids = engineQidsFor(m_currentCtxId, id);
        // 逐个实例处理
        for (const auto& qid : qids)
            // 强制写输出并传播
            m_eventEngine->triggerForceOutput(qid, 0, valStr);
        // 读线延迟
        quint64 wd = m_eventEngine->wireDelay();
        // 读门延迟
        quint64 gd = m_eventEngine->globalGateDelay();
        // 跑完传播（单次翻转，深度估计 ×8+20）
        runEventEngineFully((wd + gd) * 8 + 20, 200000);
        // 回写结果
        applyEngineOutputsToContexts();
        // 通知仿真前进
        emit simulationTick();
        // 否则迭代引擎模式
    } else {
        // 全量重算
        evaluateAll();
        // 结束当前代码块
    }
    // 通知通用数据变化
    emit changed();
    // 结束当前代码块
}

// 重置所有时钟与计数（"复位"按钮）
void Circuit::resetAllInputs() {
    // 浏览模式禁止
    if (m_viewOnly) return;
    // 复位是破坏性操作，先压撤销快照
    pushUndo();
    // 跳变计数归零
    m_clockTickCount = 0;
    // 遍历时钟状态
    for (auto it = m_clockStates.begin(); it != m_clockStates.end(); ++it)
        // 全部清为低电平
        it.value() = false;
    // 局部 lambda：把一个上下文里的 clock 输出端口重置为全 0
    auto resetCtx = [&](Context& ctx) {
        // 遍历元件
        for (int i = 0; i < ctx.components.size(); ++i) {
            // 取副本
            QVariantMap c = ctx.components[i].toMap();
            // 类型
            QString t = c.value("type").toString();
            // 位宽
            int bw = c.value("bitWidth").toInt();
            // 位宽兜底
            if (bw < 1) bw = 1;
            // 只处理时钟
            if (t == "clock") {
                // 输出数组
                QVariantList out;
                // 全 0 串 = 低电平
                out.append(QString(bw, '0'));
                // 写回 map
                c["outputPorts"] = out;
                // 写回上下文
                ctx.components[i] = c;
                // 结束当前代码块
            }
            // 结束当前代码块
        }
        // 结束 lambda 定义并补分号
    };
    // 重置根上下文
    resetCtx(m_root);
    // 遍历子电路
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        // 重置子电路里的时钟
        resetCtx(it.value());
    // 事件引擎模式
    if (m_engineType == "event" && m_eventEngine) {
        // 重建拓扑，让引擎内部状态与"全部时钟归零"一致
        rebuildEventEngine();
        // 清空事件队列与节点状态
        m_eventEngine->reset();
        // 读线延迟
        quint64 wd0 = m_eventEngine->wireDelay();
        // 读门延迟
        quint64 gd0 = m_eventEngine->globalGateDelay();
        // reset 后队列里是初始重算事件，必须先跑完再回写（否则会用默认 0 覆盖）
        runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
        // 回写结果
        applyEngineOutputsToContexts();
        // 否则迭代引擎模式
    } else {
        // 全量重算
        evaluateAll();
        // 结束当前代码块
    }
    // 通知时钟状态变化
    emit clockChanged();
    // 通知通用数据变化
    emit changed();
    // 结束当前代码块
}

// 给某一类门单独设置延迟（覆盖全局门延迟）
void Circuit::setGateTypeDelay(const QString& type, qulonglong d) {
    // 引擎不存在时无处可设
    if (!m_eventEngine) return;
    // 转发给引擎，内部按类型存到延迟覆盖表
    m_eventEngine->setGateTypeDelay(type, d);
    // 通知引擎参数面板刷新
    emit engineSettingsChanged();
    // 参数会影响仿真结果，一并通知重绘
    emit changed();
    // 结束当前代码块
}
// 查询某类门的有效延迟
qulonglong Circuit::getGateTypeDelay(const QString& type) const {
    // 引擎不存在返回 0
    if (!m_eventEngine) return 0;
    // 转发查询：有覆盖返回覆盖值，无覆盖返回全局门延迟
    return m_eventEngine->gateDelayFor(type);
    // 结束当前代码块
}
// 清空所有按类型的延迟覆盖，回到统一使用全局延迟
void Circuit::clearGateTypeDelays() {
    // 引擎为空直接返回
    if (!m_eventEngine) return;
    // 转发给引擎
    m_eventEngine->clearGateTypeDelays();
    // 通知参数面板
    emit engineSettingsChanged();
    // 通知界面刷新
    emit changed();
    // 结束当前代码块
}

// 启动事件驱动仿真
void Circuit::startSimulation() {
    // 只有事件引擎才有"仿真运行"概念
    if (m_engineType != "event") return;
    // 已在运行则幂等返回
    if (m_simulationRunning) return;
    // 需要先准备引擎的三种情况：拓扑过期 / 引擎为空 / 引擎里一个节点都没有（例如首次启动）
    if (m_engineDirty || !m_eventEngine || m_eventEngine->nodeCount() == 0) {
        // 重建拓扑
        rebuildEventEngine();
        // 读线延迟
        quint64 wd0 = m_eventEngine->wireDelay();
        // 读门延迟
        quint64 gd0 = m_eventEngine->globalGateDelay();
        // 跑完初始事件，让电路处于稳定初态
        // ★ 修复：runEventEngineFully 内部有总事件预算，振荡电路不会再卡死；
        //   最多 3 万事件后返回，剩余事件交给后续 onSimTimerTick 每帧推进。
        runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
        // 回写初态，界面上是"稳定值"而不是默认 0
        applyEngineOutputsToContexts();
        // 结束当前代码块
    }
    // 置运行标志（在准备完之后才置，保证期间其他逻辑看到的是"未运行"）
    m_simulationRunning = true;
    // 启动帧定时器（16ms 一拍，驱动 onSimTimerTick）
    if (m_simTimer) m_simTimer->start();
    // 通知运行状态变化
    emit simulationChanged();
    // 通知通用数据变化
    emit changed();
    // 结束当前代码块
}

// 暂停仿真
void Circuit::pauseSimulation() {
    // 未在运行则幂等返回
    if (!m_simulationRunning) return;
    // 先清标志：之后的所有回调都会立即返回
    m_simulationRunning = false;
    // 停掉帧定时器
    if (m_simTimer) m_simTimer->stop();
    // 时钟依赖仿真推进，暂停时一并停掉，避免时钟空转
    if (m_clockRunning) stopClock();
    // 通知运行状态变化
    emit simulationChanged();
    // 通知通用数据变化
    emit changed();
    // 结束当前代码块
}

// 重置仿真：清空事件队列与节点状态，回到初态
void Circuit::resetSimulation() {
    // 引擎为空无法重置
    if (!m_eventEngine) return;
    // 先停时钟，避免重置过程中定时器还在翻转
    if (m_clockRunning) stopClock();
    // 清事件队列、仿真时间与节点缓存
    m_eventEngine->reset();
    // ★ 修复：reset 后队列里是初始 Recalc 事件，必须先跑完再 apply
    // 读线延迟（下面用来估算收敛深度）
    quint64 wd0 = m_eventEngine->wireDelay();
    // 读门延迟
    quint64 gd0 = m_eventEngine->globalGateDelay();
    // 把 reset 产生的初始重算事件跑完，否则 apply 会用默认值覆盖显示
    // ★ 修复：runEventEngineFully 内部有总事件预算，振荡电路不会再卡死。
    runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
    // 回写结果
    applyEngineOutputsToContexts();
    // 通知仿真状态变化
    emit simulationChanged();
    // 通知时间/数值刷新
    emit simulationTick();
    // 通知通用数据变化
    emit changed();
    // 结束当前代码块
}

// ★ 修复：rebuild 后必须先跑完初始事件再 apply
// 仿真单步推进：只在事件引擎下工作，是"仿真控制面板"的入口（singleStep 则是通用单步）
void Circuit::simulateStep() {
    // 没有事件引擎就没法推进
    if (m_engineType != "event" || !m_eventEngine) return;
    // 拓扑过期先重建
    if (m_engineDirty) {
        // 重建
        rebuildEventEngine();
        // 读线延迟
        quint64 wd0 = m_eventEngine->wireDelay();
        // 读门延迟
        quint64 gd0 = m_eventEngine->globalGateDelay();
        // 跑完初始事件
        runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
        // 回写初态
        applyEngineOutputsToContexts();
        // 结束当前代码块
    }
    // 推进一个事件
    m_eventEngine->step();
    // 回写增量结果
    applyEngineOutputsToContexts();
    // 通知推进
    emit simulationTick();
    // 通知重绘
    emit changed();
    // 结束当前代码块
}

// ★ 修复：rebuild 后必须先跑完初始事件再 apply（大电路 units 小，一次 run 收敛不了）
// 仿真帧回调：每 16ms 由 m_simTimer 触发，按当前速度推进仿真
void Circuit::onSimTimerTick() {
    // 引擎不存在或仿真已暂停则不做事（暂停后残留的一次 tick 靠这里挡住）
    if (!m_eventEngine || !m_simulationRunning) return;
    // 结构变过则先重建拓扑
    if (m_engineDirty) {
        // 重建
        rebuildEventEngine();
        // 读线延迟
        quint64 wd0 = m_eventEngine->wireDelay();
        // 读门延迟
        quint64 gd0 = m_eventEngine->globalGateDelay();
        // 跑完初始事件
        // ★ 修复：runEventEngineFully 内部有总事件预算，振荡电路不会再卡死。
        runEventEngineFully((wd0 + gd0) * 64 + 200, 500000);
        // 回写初态
        applyEngineOutputsToContexts();
        // 结束当前代码块
    }
    // 决定本帧推进多少仿真时间：自动速度固定 1000；否则把"速度(单位/秒)"折算成每帧（60fps）的量
    quint64 units = m_autoSpeed ? 1000 : (m_simulationSpeed / 60);
    // 至少推进 1 单位，避免低速时永远不前进
    if (units < 1) units = 1;
    // 在 units 个时间单位内推进，事件数受 kSimRunBudget 限制，保证单帧不卡 UI
    // ★ 修复：kSimRunBudget 已降到 30000，振荡器下每帧处理耗时约 3~15ms，UI 帧率稳定 50fps 以上；
    //   多余的帧内事件会自然堆积到下一帧，视觉上是"每帧稳定推进若干个翻转"的流畅闪烁。
    m_eventEngine->run(units, kSimRunBudget);
    // 回写本帧结果（增量更新脏节点）
    applyEngineOutputsToContexts();
    // 通知仿真前进了一帧
    emit simulationTick();
    // 通知界面重绘
    emit changed();
    // 结束当前代码块
}