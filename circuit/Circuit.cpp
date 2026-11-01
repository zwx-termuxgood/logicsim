#include "Circuit.h"
// 引入本文件要实现的 Circuit 类完整声明：定义其成员函数前必须看到类定义
#include "core/Component.h"
// 引入元件存取器：集中约定元件 map 的字段名与取值方式
#include "logic/ComponentLogic.h"
// 引入元件逻辑库：查询端口几何/端口数量等的唯一来源
#include "logic/event/EventEngine.h"
// 引入事件引擎：构造函数创建它，各 getter/setter 转发它的参数
#include <QVariantMap>
// 引入 QVariantMap：本文件按需处理元件/连线 map（保持依赖一致）
#include <QString>
// 引入 QString：上下文名、引擎类型、线型等字符串
#include <QDebug>
// 引入 QDebug：调试输出用（本文件保留 include 以便后续加日志）
#include <QSettings>
// 引入 QSettings：全局设置的加载与保存

static const char* kUndoPropName = "__logicsim_undo_v1";
// 动态属性名：撤销数据指针以此键挂在 QObject 上，
// 用 void* 包装是为了不在头文件暴露 CircuitUndoData 的完整定义

// 构造函数：初始化撤销数据、最近文件、时钟定时器、事件引擎、仿真定时器与全局设置
Circuit::Circuit(QObject* parent) : QObject(parent) {
    // 新建撤销数据容器
    CircuitUndoData* d = new CircuitUndoData();
    // 以 void* 形式挂到动态属性上
    setProperty(kUndoPropName, QVariant::fromValue(static_cast<void*>(d)));

    // 从 QSettings 读取最近文件列表
    loadRecentFiles();

    // 创建时钟定时器，父子关系交给 QObject 管理生命周期
    m_clockTimer = new QTimer(this);
    // 时钟 tick 连接到回调
    connect(m_clockTimer, &QTimer::timeout, this, &Circuit::tick);

    // 创建事件引擎（非 QObject，析构函数手动 delete）
    m_eventEngine = new EventEngine();

    // 创建仿真定时器
    m_simTimer = new QTimer(this);
    // 约 60fps 一帧
    m_simTimer->setInterval(16);
    // 仿真 tick 连接到回调
    connect(m_simTimer, &QTimer::timeout, this, &Circuit::onSimTimerTick);

    // 读取引擎/速度/时钟/连线模式等全局设置
    loadSettings();
}

// 析构函数：释放撤销数据与事件引擎
Circuit::~Circuit() {
    // 取撤销数据指针
    CircuitUndoData* d = und();
    // 存在则释放
    if (d) {
        // 删除撤销数据
        delete d;
        // 清空动态属性，避免悬空指针
        setProperty(kUndoPropName, QVariant());
    }
    // 事件引擎非 QObject 子对象，需手动删除
    if (m_eventEngine) { delete m_eventEngine; m_eventEngine = nullptr; }
}

// 定义"加载全局设置"接口：读取引擎/速度/时钟/连线模式，带默认值与钳位
void Circuit::loadSettings() {
    // 打开默认 QSettings
    QSettings s;
    // 读取引擎类型，非法值回退 event
    QString et = s.value("sim/engineType", "event").toString();
    m_engineType = (et == "iter") ? "iter" : "event";

    // 事件引擎存在才应用延迟设置
    if (m_eventEngine) {
        // 读取全局门延迟，下限 1
        qulonglong gd = s.value("sim/globalGateDelay", 1).toULongLong();
        if (gd < 1) gd = 1;
        m_eventEngine->setGlobalGateDelay(gd);

        // 读取线延迟，下限 1
        qulonglong wd = s.value("sim/wireDelay", 1).toULongLong();
        if (wd < 1) wd = 1;
        m_eventEngine->setWireDelay(wd);
    }

    // 读取速度指数，钳位到 0..12
    m_simulationSpeedExp = qBound(0, s.value("sim/simulationSpeedExp", 3).toInt(), 12);
    // 由指数换算实际速度：10^exp
    qulonglong sp = 1;
    for (int i = 0; i < m_simulationSpeedExp; ++i) sp *= 10;
    m_simulationSpeed = sp;

    // 读取自动速度标志
    m_autoSpeed      = s.value("sim/autoSpeed", true).toBool();
    // 读取是否显示仿真时间
    m_showSimTime    = s.value("sim/showSimTime", false).toBool();
    // 读取时钟频率，钳位到 1..1000
    m_clockFrequency = qBound(1, s.value("sim/clockFrequency", 2).toInt(), 1000);

    // 读取连线模式，只接受三种合法值
    m_wireMode = s.value("sim/wireMode", "bezier").toString();
    if (m_wireMode != "bezier" && m_wireMode != "ortho" && m_wireMode != "switchable")
        m_wireMode = "bezier";
    // 读取当前线型，只接受 bezier/ortho
    m_currentWireType = s.value("sim/currentWireType", "bezier").toString();
    if (m_currentWireType != "bezier" && m_currentWireType != "ortho")
        m_currentWireType = "bezier";
}

// 定义"保存全局设置"接口
void Circuit::saveSettings() {
    // 打开默认 QSettings
    QSettings s;
    // 保存引擎类型
    s.setValue("sim/engineType", m_engineType);
    // 保存全局门延迟（引擎为空时用 1 兜底）
    s.setValue("sim/globalGateDelay",
               m_eventEngine ? m_eventEngine->globalGateDelay() : qulonglong(1));
    // 保存线延迟
    s.setValue("sim/wireDelay",
               m_eventEngine ? m_eventEngine->wireDelay() : qulonglong(1));
    // 保存速度指数
    s.setValue("sim/simulationSpeedExp", m_simulationSpeedExp);
    // 保存自动速度标志
    s.setValue("sim/autoSpeed", m_autoSpeed);
    // 保存是否显示仿真时间
    s.setValue("sim/showSimTime", m_showSimTime);
    // 保存时钟频率
    s.setValue("sim/clockFrequency", m_clockFrequency);
    // 保存连线模式
    s.setValue("sim/wireMode", m_wireMode);
    // 保存当前线型
    s.setValue("sim/currentWireType", m_currentWireType);
}

// 定义"设置是否显示仿真时间"接口
void Circuit::setShowSimTime(bool b) {
    // 值未变则直接返回
    if (m_showSimTime == b) return;
    // 更新成员
    m_showSimTime = b;
    // 持久化设置
    saveSettings();
    // 通知 QML 刷新
    emit showSimTimeChanged();
}

// 定义"设置连线模式"接口
void Circuit::setWireMode(const QString& m) {
    // 值未变则返回
    if (m_wireMode == m) return;
    // 只接受三种合法值，非法值忽略
    if (m != "bezier" && m != "ortho" && m != "switchable") return;
    // 更新成员
    m_wireMode = m;
    // 持久化设置
    saveSettings();
    // 通知 QML 刷新
    emit wireModeChanged();
}

// 定义"设置当前线型"接口
void Circuit::setCurrentWireType(const QString& t) {
    // 值未变则返回
    if (m_currentWireType == t) return;
    // 只接受 bezier/ortho，非法值忽略
    if (t != "bezier" && t != "ortho") return;
    // 更新成员
    m_currentWireType = t;
    // 持久化设置
    saveSettings();
    // 通知 QML 刷新
    emit wireModeChanged();
}

// 定义"综合出实际生效线型"接口
QString Circuit::effectiveWireType() const {
    // ortho 模式强制 ortho
    if (m_wireMode == "ortho") return "ortho";
    // bezier 模式强制 bezier
    if (m_wireMode == "bezier") return "bezier";
    // switchable 模式用当前线型
    return m_currentWireType;
}

// 定义"取出撤销数据指针"接口
CircuitUndoData* Circuit::und() const {
    // 从动态属性读 void* 并转回具体类型；不存在则为 nullptr
    return static_cast<CircuitUndoData*>(property(kUndoPropName).value<void*>());
}

// 定义"是否可撤销"接口
bool Circuit::canUndo() const {
    // 取撤销数据
    CircuitUndoData* d = und();
    // 撤销栈非空即可撤销
    return d && !d->undoStack.empty();
}

// 定义"是否可重做"接口
bool Circuit::canRedo() const {
    // 取撤销数据
    CircuitUndoData* d = und();
    // 重做栈非空即可重做
    return d && !d->redoStack.empty();
}

// 定义"打印内部状态"调试辅助接口（当前为空实现）
void Circuit::dumpInternalState(const QString& where) const { Q_UNUSED(where); }

// 定义"取当前上下文引用"接口（非 const）
Circuit::Context& Circuit::currentCtx() {
    // 根上下文：id 为空
    if (m_currentCtxId.isEmpty()) return m_root;
    // 查子电路表
    auto it = m_subContexts.find(m_currentCtxId);
    // 找不到则回退根
    if (it == m_subContexts.end()) return m_root;
    // 返回子电路上下文
    return it.value();
}
// 定义"取当前上下文引用"接口（const）
const Circuit::Context& Circuit::currentCtx() const {
    // 根上下文：id 为空
    if (m_currentCtxId.isEmpty()) return m_root;
    // 查子电路表
    auto it = m_subContexts.find(m_currentCtxId);
    // 找不到则回退根
    if (it == m_subContexts.end()) return m_root;
    // 返回子电路上下文
    return it.value();
}
// 定义"按 id 查上下文指针"接口（非 const）
Circuit::Context* Circuit::ctxById(const QString& id) {
    // 空 id 表示根上下文
    if (id.isEmpty()) return &m_root;
    // 查子电路表
    auto it = m_subContexts.find(id);
    // 不存在返回空指针
    if (it == m_subContexts.end()) return nullptr;
    // 返回子电路上下文指针
    return &it.value();
}
// 定义"按 id 查上下文指针"接口（const）
const Circuit::Context* Circuit::ctxById(const QString& id) const {
    // 空 id 表示根上下文
    if (id.isEmpty()) return &m_root;
    // 查子电路表
    auto it = m_subContexts.find(id);
    // 不存在返回空指针
    if (it == m_subContexts.end()) return nullptr;
    // 返回子电路上下文指针
    return &it.value();
}

// 定义"返回当前上下文元件列表"接口
QVariantList Circuit::components() const { return currentCtx().components; }
// 定义"返回当前上下文连线列表"接口
QVariantList Circuit::wires() const { return currentCtx().wires; }

// 定义"返回当前上下文显示名"接口
QString Circuit::contextName() const {
    // 根上下文固定为"主电路"
    if (m_currentCtxId.isEmpty()) return "主电路";
    // 子电路取名称，缺失则用默认名
    QString n = m_subNames.value(m_currentCtxId, "子电路");
    // 浏览模式追加标记
    if (m_viewOnly) n += "（浏览）";
    // 返回显示名
    return n;
}

// 定义"在当前上下文查元件索引"接口
int Circuit::indexOfComponent(const QString& id) const {
    // 委托上下文自身的线性查找
    return currentCtx().indexOfComponent(id);
}
// 定义"在当前上下文查连线索引"接口
int Circuit::indexOfWire(const QString& id) const {
    // 委托上下文自身的线性查找
    return currentCtx().indexOfWire(id);
}

// 定义"返回仿真时间"接口
qulonglong Circuit::simTime() const {
    // 引擎为空时返回 0
    return m_eventEngine ? m_eventEngine->time() : 0;
}
// 定义"返回全局门延迟"接口
qulonglong Circuit::globalGateDelay() const {
    // 引擎为空时返回 1
    return m_eventEngine ? m_eventEngine->globalGateDelay() : 1;
}
// 定义"设置全局门延迟"接口
void Circuit::setGlobalGateDelay(qulonglong d) {
    // 引擎不存在则无法设置
    if (!m_eventEngine) return;
    // 下限钳到 1
    if (d < 1) d = 1;
    // 值未变则返回
    if (m_eventEngine->globalGateDelay() == d) return;
    // 转发给引擎
    m_eventEngine->setGlobalGateDelay(d);
    // 持久化设置
    saveSettings();
    // 通知 QML 引擎设置变化
    emit engineSettingsChanged();
}
// 定义"返回线延迟"接口
qulonglong Circuit::wireDelay() const {
    // 引擎为空时返回 1
    return m_eventEngine ? m_eventEngine->wireDelay() : 1;
}
// 定义"设置线延迟"接口
void Circuit::setWireDelay(qulonglong d) {
    // 引擎不存在则无法设置
    if (!m_eventEngine) return;
    // 下限钳到 1
    if (d < 1) d = 1;
    // 值未变则返回
    if (m_eventEngine->wireDelay() == d) return;
    // 转发给引擎
    m_eventEngine->setWireDelay(d);
    // 持久化设置
    saveSettings();
    // 通知 QML 引擎设置变化
    emit engineSettingsChanged();
}
// 定义"设置仿真速度数值"接口
void Circuit::setSimulationSpeed(qulonglong s) {
    // 值未变则返回
    if (m_simulationSpeed == s) return;
    // 更新成员
    m_simulationSpeed = s;
    // 持久化设置
    saveSettings();
    // 通知 QML 引擎设置变化
    emit engineSettingsChanged();
}
// 定义"设置仿真速度指数"接口
void Circuit::setSimulationSpeedExp(int e) {
    // 下限钳到 0
    if (e < 0) e = 0;
    // 上限钳到 12
    if (e > 12) e = 12;
    // 值未变则返回
    if (m_simulationSpeedExp == e) return;
    // 更新指数
    m_simulationSpeedExp = e;
    // 由指数换算实际速度：10^e
    qulonglong v = 1;
    for (int i = 0; i < e; ++i) v *= 10;
    m_simulationSpeed = v;
    // 持久化设置
    saveSettings();
    // 通知 QML 引擎设置变化
    emit engineSettingsChanged();
}
// 定义"设置自动速度"接口
void Circuit::setAutoSpeed(bool a) {
    // 值未变则返回
    if (m_autoSpeed == a) return;
    // 更新成员
    m_autoSpeed = a;
    // 持久化设置
    saveSettings();
    // 通知 QML 引擎设置变化
    emit engineSettingsChanged();
}