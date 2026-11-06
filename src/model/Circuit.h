#ifndef CIRCUIT_H
// 头文件保护：防止同一个编译单元中多次包含本文件导致重复定义
#define CIRCUIT_H
// 定义宏，与 #ifndef 配对，标记本头文件已被包含

#include <QObject>
// 引入 QObject 基类：Circuit 继承它才能使用信号槽、Q_PROPERTY、Q_INVOKABLE 等 Qt 元对象特性
#include <QVariantList>
// 引入 QVariantList（即 QList<QVariant>）：用于承载元件列表、连线列表这类同构但字段不同的数据
#include <QVariantMap>
// 引入 QVariantMap（即 QMap<QString,QVariant>）：每个元件/连线都用一个 map 表示，字段名作为 key
#include <QString>
// 引入 QString：Qt 的 Unicode 字符串，用于 ID、名称、类型等
#include <QStringList>
// 引入 QStringList：用于批量 ID 参数（removeComponents、copySelection 等）
#include <QHash>
// 引入 QHash：用于 m_subContexts、m_subNames、m_clockStates 等键值容器
#include <QSet>
// 引入 QSet：用于 hasSubCycle 的已访问集合、去重判断
#include <QVector>
// 引入 QVector：用于 m_browseStack 浏览栈
#include <QTimer>
// 引入 QTimer：时钟定时器 m_clockTimer 与仿真定时器 m_simTimer 需要它
#include <QDateTime>
// 引入 QDateTime：撤销合并键的时间戳判断需要它

#include "core/CircuitContext.h"
// 引入电路上下文结构体：Circuit 内部用 Context（= CircuitContext）存放一组元件与连线
#include "core/UndoData.h"
// 引入撤销数据结构：CircuitUndoData、UndoEntry、UndoScope 在成员函数签名中使用

class EventEngine;
// 前置声明事件引擎类：头文件里只用到指针 EventEngine*，无需完整定义，减少编译依赖

class Circuit : public QObject
// Circuit 继承 QObject：获得元对象系统（信号槽、属性、Q_INVOKABLE 反射）
{
    Q_OBJECT
    // Qt 宏：必须放在类声明内，moc 会据此生成元对象代码（信号槽、属性表）
    Q_DISABLE_COPY_MOVE(Circuit)
    // 禁用拷贝构造、拷贝赋值、移动构造、移动赋值：Circuit 持有大量资源与定时器，拷贝没有语义


    /*```cpp
Q_PROPERTY(QVariantList components READ components NOTIFY changed)
```

逐 token 看：

· Q_PROPERTY —— 宏名，固定。
· QVariantList —— 属性的 C++ 类型。必须是 moc 能识别的类型（Qt 内置类型或已用 Q_DECLARE_METATYPE 注册的类型）。
· components —— 属性的 名字，字符串层面就是这个。QML 里写 obj.components 用的就是它，和 property("components") 的 key 一致。
· READ components —— 指定读取函数名为 components。注意：这里写的是函数名，不是调用。moc 会去你的类里找 QVariantList components() const 这个函数。这个函数是真实存在的普通 C++ 函数，QML 读属性时引擎通过元表找到它并调用。
· NOTIFY changed —— 指定通知信号名为 changed。同理，moc 找类里的 void changed() 信号，把"属性变化 → 发这个信号"登记进元表。*/


    Q_PROPERTY(QVariantList components READ components NOTIFY changed)
    // 暴露给 QML 的只读属性 components：读取调用 components()，数据变化时发 changed 信号
    Q_PROPERTY(QVariantList wires READ wires NOTIFY changed)
    // 暴露连线列表给 QML，复用 changed 信号通知刷新
    Q_PROPERTY(QString contextId READ contextId NOTIFY contextChanged)
    // 当前上下文 ID（根电路为空串，子电路为其 S 编号），变化时发 contextChanged
    Q_PROPERTY(QString contextName READ contextName NOTIFY contextChanged)
    // 当前上下文显示名（主电路/子电路名，浏览模式追加“（浏览）”）
    Q_PROPERTY(bool isRootContext READ isRootContext NOTIFY contextChanged)
    // 是否处于根上下文：QML 据此决定是否显示“返回主电路”按钮
    Q_PROPERTY(bool isViewOnly READ isViewOnly NOTIFY contextChanged)
    // 是否处于只读浏览模式：浏览子电路实例时禁止编辑
    Q_PROPERTY(QVariantList subcircuits READ subcircuits NOTIFY subcircuitsChanged)
    // 所有子电路列表（含输入输出端口数统计），供 QML 侧边栏展示
    Q_PROPERTY(QVariantList editContexts READ editContexts NOTIFY editContextsChanged)
    // 可编辑上下文列表（根 + 所有子电路），供“切换编辑上下文”下拉框使用
    Q_PROPERTY(QVariantList errors READ errors NOTIFY errorsChanged)
    // 错误列表（位宽不匹配等），QML 据此展示错误面板
    Q_PROPERTY(QVariantList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    // 最近打开文件列表，供“最近文件”菜单
    Q_PROPERTY(bool clockRunning READ clockRunning NOTIFY clockChanged)
    // 时钟是否在运行，QML 据此切换播放/暂停按钮图标
    Q_PROPERTY(int clockFrequency READ clockFrequency NOTIFY clockChanged)
    // 时钟频率（Hz），只读暴露给 QML 显示
    Q_PROPERTY(int clockTickCount READ clockTickCount NOTIFY clockChanged)
    // 时钟已跳变次数，用于显示或调试
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoRedoChanged)
    // 是否可以撤销，QML 据此禁用撤销按钮
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoRedoChanged)
    // 是否可以重做，QML 据此禁用重做按钮
    Q_PROPERTY(QString engineType READ engineType WRITE setEngineType NOTIFY engineChanged)
    // 引擎类型（"event"/"iter"）：可读可写，切换后发 engineChanged
    Q_PROPERTY(bool simulationRunning READ simulationRunning NOTIFY simulationChanged)
    // 仿真是否运行中
    Q_PROPERTY(qulonglong simTime READ simTime NOTIFY simulationTick)
    // 仿真时间戳（事件引擎的 m_time），每 tick 通知刷新
    Q_PROPERTY(qulonglong globalGateDelay READ globalGateDelay WRITE setGlobalGateDelay NOTIFY engineSettingsChanged)
    // 全局门延迟：可读写，修改后保存设置并发 engineSettingsChanged
    Q_PROPERTY(qulonglong wireDelay READ wireDelay WRITE setWireDelay NOTIFY engineSettingsChanged)
    // 线延迟：同上
    Q_PROPERTY(qulonglong simulationSpeed READ simulationSpeed WRITE setSimulationSpeed NOTIFY engineSettingsChanged)
    // 仿真速度（每帧推进的仿真时间单位）：同上
    Q_PROPERTY(int simulationSpeedExp READ simulationSpeedExp WRITE setSimulationSpeedExp NOTIFY engineSettingsChanged)
    // 仿真速度的 10 的指数形式（0..12），便于 UI 用滑条整数表示
    Q_PROPERTY(bool autoSpeed READ autoSpeed WRITE setAutoSpeed NOTIFY engineSettingsChanged)
    // 是否自动速度：true 时每帧固定推进 1000 单位
    Q_PROPERTY(bool showSimTime READ showSimTime WRITE setShowSimTime NOTIFY showSimTimeChanged)
    // 是否显示仿真时间，独立信号 showSimTimeChanged 因为不涉及引擎设置
    Q_PROPERTY(QString wireMode READ wireMode WRITE setWireMode NOTIFY wireModeChanged)
    // 连线模式（"bezier"/"ortho"/"switchable"）：全局策略
    Q_PROPERTY(QString currentWireType READ currentWireType WRITE setCurrentWireType NOTIFY wireModeChanged)
    // switchable 模式下用户当前选择的线型
    Q_PROPERTY(QString effectiveWireType READ effectiveWireType NOTIFY wireModeChanged)
    // 综合 wireMode 与 currentWireType 得到的实际生效线型，供 QML 绘制使用

public:
    // 公有接口：构造/析构、QML 可直接调用的 Q_INVOKABLE 方法、属性 getter/setter

    explicit Circuit(QObject* parent = nullptr);
    // explicit 构造函数：防止隐式转换；parent 交给 QObject 管理生命周期
    ~Circuit() override;
    // 析构函数：清理 undo 数据、事件引擎等非 QObject 子对象

    QVariantList components() const;
    // 返回当前上下文的元件列表（每次拷贝一份 QVariantList，QML 侧只读使用）
    QVariantList wires() const;
    // 返回当前上下文的连线列表
    QString contextId() const { return m_currentCtxId; }
    // 内联返回当前上下文 ID：根为空串，子电路为 S 编号
    QString contextName() const;
    // 计算显示名：需要查 m_subNames 并附加浏览标记，故在 cpp 中实现
    bool isRootContext() const { return m_currentCtxId.isEmpty(); }
    // 判断是否根上下文：约定用空串表示根
    bool isViewOnly() const { return m_viewOnly; }
    // 返回只读浏览标志
    QVariantList subcircuits() const;
    // 遍历 m_subNames 统计每个子电路的 input/output 数量，组装成 QVariantList
    QVariantList editContexts() const;
    // 组装“根 + 所有子电路”的上下文列表，供编辑切换器使用
    QVariantList errors() const { return m_errors; }
    // 直接返回缓存的错误列表，错误在 refreshErrors 中重新计算
    QVariantList recentFiles() const { return m_recentFiles; }
    // 直接返回缓存的最近文件列表
    bool clockRunning() const { return m_clockRunning; }
    // 返回时钟运行标志
    int clockFrequency() const { return m_clockFrequency; }
    // 返回时钟频率
    int clockTickCount() const { return m_clockTickCount; }
    // 返回时钟跳变计数
    bool canUndo() const;
    // 检查撤销栈非空：需要访问 CircuitUndoData，故在 cpp 实现
    bool canRedo() const;
    // 检查重做栈非空

    QString engineType() const { return m_engineType; }
    // 返回引擎类型字符串
    void setEngineType(const QString& t);
    // 切换引擎：暂停仿真、停止时钟、保存设置、重建引擎、全量刷新
    bool simulationRunning() const { return m_simulationRunning; }
    // 返回仿真运行标志
    qulonglong simTime() const;
    // 返回事件引擎当前时间：引擎指针可能为空，故在 cpp 实现
    qulonglong globalGateDelay() const;
    // 返回事件引擎的全局门延迟
    void setGlobalGateDelay(qulonglong d);
    // 设置全局门延迟：下限 1，保存设置并发 engineSettingsChanged
    qulonglong wireDelay() const;
    // 返回事件引擎的线延迟
    void setWireDelay(qulonglong d);
    // 设置线延迟：下限 1，保存设置并发 engineSettingsChanged
    qulonglong simulationSpeed() const { return m_simulationSpeed; }
    // 返回仿真速度（每帧推进单位数）
    void setSimulationSpeed(qulonglong s);
    // 直接设置速度数值：保存设置并发 engineSettingsChanged
    int simulationSpeedExp() const { return m_simulationSpeedExp; }
    // 返回速度指数（0..12）
    void setSimulationSpeedExp(int e);
    // 设置速度指数：钳位到 0..12，换算成 simulationSpeed 后保存
    bool autoSpeed() const { return m_autoSpeed; }
    // 返回自动速度标志
    void setAutoSpeed(bool a);
    // 设置自动速度：保存设置并发 engineSettingsChanged
    bool showSimTime() const { return m_showSimTime; }
    // 返回是否显示仿真时间
    void setShowSimTime(bool b);
    // 设置显示仿真时间：保存设置并发 showSimTimeChanged

    QString wireMode() const { return m_wireMode; }
    // 返回连线模式
    void setWireMode(const QString& m);
    // 设置连线模式：仅接受 bezier/ortho/switchable，非法值忽略
    QString currentWireType() const { return m_currentWireType; }
    // 返回 switchable 模式下的当前线型
    void setCurrentWireType(const QString& t);
    // 设置当前线型：仅接受 bezier/ortho
    
    /*
    Q_INVOKABLE 是 Qt 的宏，写在成员函数声明前，作用是把普通成员函数注册进元对象系统
    */
    Q_INVOKABLE QString effectiveWireType() const;
    // 综合出实际生效线型：ortho 模式强制 ortho，bezier 模式强制 bezier，否则用 currentWireType

    Q_INVOKABLE void setGateTypeDelay(const QString& type, qulonglong d);
    // 设置某种门类型的延迟：转发给事件引擎，并发信号
    Q_INVOKABLE qulonglong getGateTypeDelay(const QString& type) const;
    // 查询某种门类型的延迟：转发给事件引擎
    Q_INVOKABLE void clearGateTypeDelays();
    // 清空所有按类型的延迟覆盖，回到全局延迟
    Q_INVOKABLE void startSimulation();
    // 启动事件驱动仿真：必要时先 rebuild + 收敛，然后启动定时器
    Q_INVOKABLE void pauseSimulation();
    // 暂停仿真：停止定时器并同时停止时钟
    Q_INVOKABLE void resetSimulation();
    // 重置仿真：清空事件队列、恢复输入、重新收敛并 apply
    Q_INVOKABLE void simulateStep();
    // 单步仿真：推进一个事件或一次时钟 tick（依引擎类型而定）

    Q_INVOKABLE QString addComponent(const QString& type, double x, double y,
                                     int bitWidth = 1, int inputCount = 2,
                                     const QString& subId = QString());
    // 添加元件：分配 ID、初始化默认字段与端口、插入当前上下文，返回新 ID
    Q_INVOKABLE void removeComponent(const QString& id);
    // 删除单个元件：同时移除关联连线，若端口位置有 T 型接入也一并移除
    Q_INVOKABLE void removeComponents(const QStringList& ids);
    // 批量删除：先收集端口位置，再一次性移除元件与相关连线，减少重复遍历
    Q_INVOKABLE void moveComponent(const QString& id, double x, double y);
    // 移动元件到绝对坐标：更新 ortho 线端点并触发几何/结构信号
    Q_INVOKABLE void moveSelection(const QStringList& ids, double dx, double dy);
    // 批量相对移动：逐元件平移后统一更新各自连接的 ortho 线
    Q_INVOKABLE void moveComponentWithWires(const QString& id, double x, double y);
    // 语义化接口：当前实现直接调用 moveComponent（ortho 线随动已内置）
    Q_INVOKABLE void moveSelectionWithWires(const QStringList& ids, double dx, double dy);
    // 语义化接口：批量移动并随动连线，内部转调 moveSelection
    Q_INVOKABLE void toggleInput(const QString& id);
    // 翻转 input 元件的所有位：若仿真中则触发引擎重算并收敛
    Q_INVOKABLE void setBitValue(const QString& id, int bit, bool value);
    // 设置 input/const 元件某一位的值：仿真中触发引擎单 bit 更新
    Q_INVOKABLE void setComponentProp(const QString& id, const QString& key, const QVariant& value);
    // 通用属性设置：对 bitWidth/inputCount/rotation 做钳位与联动修正，其余直接赋值
    Q_INVOKABLE void renameComponent(const QString& id, const QString& name);
    // 重命名元件：仅改 name 字段，不触发结构变化
    Q_INVOKABLE void setSplitterSplits(const QString& id, const QString& splitsStr);
    // 设置 splitter/hub 的分段：解析逗号/空格分隔整数，校验总和等于位宽
    Q_INVOKABLE QString getSplitterSplitsStr(const QString& id) const;
    // 返回分段字符串（逗号分隔），供 QML 编辑框回显
    Q_INVOKABLE void rotateComponent(const QString& id, int delta);
    // 按 90 度增量旋转：rotation 取模 4，发结构信号
    Q_INVOKABLE void toggleClock(const QString& id);
    // 手动翻转 clock 元件输出：仿真中触发引擎 forceOutput 并收敛
    Q_INVOKABLE QString addText(double x, double y, const QString& text = QString("文本"));
    // 添加文本元件：先 addComponent("text")，再 setTextContent 设置内容
    Q_INVOKABLE void setTextContent(const QString& id, const QString& content);
    // 设置文本元件内容：仅 text 类型有效

    Q_INVOKABLE QString getInputBase(const QString& id) const;
    // 返回 input/const 的显示进制：字段缺失时回退 "bin"
    Q_INVOKABLE void setInputBase(const QString& id, const QString& base);
    // 设置显示进制：影响 getInputAsString 与输入框解析
    Q_INVOKABLE QString getInputAsString(const QString& id) const;
    // 按当前显示进制格式化 input 元件值：委托 ValueCodec::format
    Q_INVOKABLE QString setInputFromString(const QString& id, const QString& text);
    // 解析文本并写入 inputBits：出错返回错误消息，成功返回空串
    Q_INVOKABLE QString formatValue(const QString& binStr, const QString& base, int bitWidth) const;
    // 纯函数：把二进制串按指定进制格式化，供 QML 展示端口值

    Q_INVOKABLE QString addWire(const QString& fromComp, int fromPort,
                                const QString& toComp, int toPort);
    // 添加 bezier 线：只记录两端元件与端口索引，几何由两端坐标实时计算
    Q_INVOKABLE QString addOrthoWire(double fromX, double fromY,
                                     double toX, double toY,
                                     int direction);
    // 添加正交线：先吸附端口，再按方向生成 L 型 path；反向时自动交换端点
    Q_INVOKABLE bool shortenWireEnd(const QString& wireId, double newX, double newY);
    // 缩短线的一端到新坐标：用于“从已有线拉出新线”后截断原线
    Q_INVOKABLE void removeWire(const QString& id);
    // 删除指定连线
    Q_INVOKABLE QString wireAtPoint(double x, double y, double tol) const;
    // 命中测试：ortho 用线段距离，bezier 用采样点距离，返回最近线 ID
    Q_INVOKABLE bool setWirePathPoint(const QString& id, int index, double x, double y);
    // 拖动 ortho 线中间拐点：网格化后同时修正相邻点以保持正交
    Q_INVOKABLE QVariantList getWirePath(const QString& id) const;
    // 返回 ortho 线的 path 顶点列表，bezier 线通常为空
    Q_INVOKABLE QString getWireType(const QString& id) const;
    // 返回线型：字段缺失时回退 "bezier"

    Q_INVOKABLE QVariantMap findPortAt(double x, double y, double tol) const;
    // 端口命中：在容差内找最近的输入/输出端口，返回 compId/port/isOutput/坐标
    Q_INVOKABLE QVariantList allWireSegments() const;
    // 枚举当前上下文所有 ortho 线段，供 QML 绘制或碰撞检测
    Q_INVOKABLE QVariantList connectionDots() const;
    // 计算连接圆点位置：委托 WireNetworkSolver，连接数 >= 3 才返回

    Q_INVOKABLE QVariantMap getComponent(const QString& id) const;
    // 按 ID 查元件 map，不存在返回空 map
    Q_INVOKABLE QVariantMap getWire(const QString& id) const;
    // 按 ID 查连线 map
    Q_INVOKABLE QVariantList outputPortsInfo(const QString& id) const;
    // 返回元件输出端口几何信息：委托 ComponentLogic::outputPorts
    Q_INVOKABLE QVariantList inputPortsInfo(const QString& id) const;
    // 返回元件输入端口几何信息
    Q_INVOKABLE QVariantMap allPortsInfo() const;
    // 一次性返回所有元件的端口信息：QML 连线时的批量查询入口
    Q_INVOKABLE QVariantMap statistics() const;
    // 统计各类元件数量：递归遍历根与子电路，避免循环引用重复计数

    Q_INVOKABLE QString addSubcircuit(const QString& name);
    // 新建空子电路上下文：分配 S 编号，插入 m_subContexts/m_subNames
    Q_INVOKABLE void deleteSubcircuit(const QString& subId);
    // 删除子电路：移除上下文、名称，并递归删除所有上下文里引用它的 sub 实例
    Q_INVOKABLE void setSubcircuitName(const QString& subId, const QString& name);
    // 修改子电路显示名：仅改 m_subNames
    Q_INVOKABLE QString getSubcircuitName(const QString& subId) const;
    // 查询子电路名
    Q_INVOKABLE bool canAddSubInstance(const QString& targetSubId) const;
    // 判断能否在当前上下文放置该子电路实例：防止自包含与循环引用
    Q_INVOKABLE void setSubcircuitAsRoot(const QString& subId);
    // 把子电路上下文与根上下文互换：实现“将子电路设为主电路”
    Q_INVOKABLE void enterSubcircuit(const QString& subId);
    // 进入子电路浏览：压栈当前状态，计算实例路径，从父实例同步输入状态
    Q_INVOKABLE void leaveSubcircuit();
    // 退出子电路浏览：弹栈恢复上一状态，必要时同步再求值
    Q_INVOKABLE void switchEditContext(const QString& ctxId);
    // 直接切换编辑上下文：清空浏览栈，重建/求值引擎

    Q_INVOKABLE void refreshErrors();
    // 重新扫描所有上下文连线，生成位宽不匹配等错误列表
    Q_INVOKABLE QVariantList copySelection(const QStringList& ids);
    // 复制选中元件及相关连线：结果缓存到 m_clipboardCache 并返回
    Q_INVOKABLE QStringList pasteSelection(const QVariantList& data, double dx, double dy);
    // 粘贴：参数 data 实际被忽略，使用内部缓存；返回新元件 ID 列表
    Q_INVOKABLE void addRecentFile(const QString& path);
    // 记录最近文件：去重、置顶、截断到 10 条、持久化
    Q_INVOKABLE void removeRecentFile(const QString& path);
    // 移除单条最近文件记录
    Q_INVOKABLE void clearRecentFiles();
    // 清空最近文件列表
    Q_INVOKABLE bool importFromFile(const QString& path);
    // 导入外部电路文件为子电路：加前缀重命名所有 ID，注册到 m_importedFiles
    Q_INVOKABLE bool removeImportedFile(const QString& path);
    // 移除某次导入产生的所有子电路：按前缀批量清理
    Q_INVOKABLE QVariantList importedFiles() const;
    // 返回已导入文件信息列表，供 UI 展示与卸载
    Q_INVOKABLE void clear();
    // 清空整个电路：重置根、子电路、时钟、引擎、剪贴板等所有状态
    Q_INVOKABLE void evaluate();
    // 触发一次全量求值：等价于 evaluateAll()
    Q_INVOKABLE QVariantMap toJson() const;
    // 序列化为 QVariantMap：委托 JsonCodec::encode，收集全部成员状态
    Q_INVOKABLE bool fromJson(const QVariantMap& data);
    // 从 QVariantMap 反序列化：委托 JsonCodec::decode 并填充成员，重建引擎
    Q_INVOKABLE bool saveToFile(const QString& path);
    // 保存到文件：toJson -> QJsonDocument -> 写文件，并加入最近文件
    Q_INVOKABLE bool loadFromFile(const QString& path);
    // 从文件加载：解析 JSON（容错提取首个对象），fromJson，必要时修复文件
    Q_INVOKABLE void startClock();
    // 启动时钟定时器：事件引擎模式下若仿真未运行则拒绝启动
    Q_INVOKABLE void stopClock();
    // 停止时钟定时器
    Q_INVOKABLE void setClockFrequency(int hz);
    // 设置时钟频率：钳位 1..1000，运行中则重启定时器
    Q_INVOKABLE void singleStep();
    // 单步：事件引擎推进一个事件并 apply；迭代引擎则走一次 tick
    Q_INVOKABLE void resetAllInputs();
    // 重置所有输入与时钟状态为 0，并重新收敛
    Q_INVOKABLE void undo();
    // 撤销：弹出撤销栈顶，捕获当前状态入重做栈，应用快照
    Q_INVOKABLE void redo();
    // 重做：弹出重做栈顶，捕获当前状态入撤销栈，应用快照
    Q_INVOKABLE void breakUndoMerge();
    // 打断撤销合并：清空 lastMergeKey，使下一次操作一定新建快照

signals:
    // 信号区：QML 通过 NOTIFY 绑定这些信号刷新属性

    void changed();
    // 通用数据变化：大部分写操作都会发，QML 据此重绘
    void geometryChanged();
    // 几何变化（移动元件、改线 path）：QML 可只重算布局
    void contextChanged();
    // 当前上下文切换（进入/离开子电路、切换编辑上下文）
    void subcircuitsChanged();
    // 子电路列表或名称变化
    void editContextsChanged();
    // 可编辑上下文列表变化（新建/删除子电路）
    void errorsChanged();
    // 错误列表变化
    void recentFilesChanged();
    // 最近文件列表变化
    void clockChanged();
    // 时钟运行状态/频率/计数变化
    void undoRedoChanged();
    // 撤销/重做栈变化：更新 canUndo/canRedo
    void engineChanged();
    // 引擎类型切换
    void simulationChanged();
    // 仿真运行/暂停状态变化
    void simulationTick();
    // 仿真推进一次：QML 刷新 simTime 与显示
    void engineSettingsChanged();
    // 引擎参数（延迟/速度）变化
    void showSimTimeChanged();
    // 显示仿真时间开关变化
    void structureChanged();
    // 结构变化（增删元件/连线、属性结构变化）：QML 需重建连线网络等
    void wireModeChanged();
    // 连线模式或当前线型变化

private slots:
    // 私有槽：仅内部定时器连接

    void tick();
    // 时钟定时器回调：翻转所有 clock 输出并触发一次求值

private:
    // 私有区：内部状态与辅助函数

    using Context = CircuitContext;
    // 类型别名：简化后续 m_root、m_subContexts 等声明

    struct BrowseFrame {
        QString ctxId;
        // 进入子电路前的上下文 ID
        bool    viewOnly;
        // 进入前是否已是只读浏览
        QString instancePath;
        // 进入前的实例路径（用于状态同步）
    };
    // 浏览栈帧：记录进入子电路前的三项状态，用于 leaveSubcircuit 恢复

    Context m_root;
    // 根上下文：主电路的元件与连线
    QHash<QString, Context> m_subContexts;
    // 所有子电路上下文：key 为 S 编号
    QHash<QString, QString> m_subNames;
    // 子电路显示名：key 为 S 编号
    QString m_currentCtxId;
    // 当前上下文 ID：空串表示根，否则为子电路 S 编号
    bool m_viewOnly = false;
    // 是否只读浏览模式：浏览子电路实例时为 true
    int m_subCounter = 0;
    // 子电路编号计数器：每次 addSubcircuit 自增
    int m_importCounter = 0;
    // 导入前缀计数器：保证每次导入前缀唯一
    QVariantList m_errors;
    // 缓存的错误列表
    QVariantList m_recentFiles;
    // 缓存的最近文件列表
    QHash<QString, QVariantMap> m_importedFiles;
    // 已导入文件信息：key 为路径，value 含 prefix/baseName/subIds

    QVector<BrowseFrame> m_browseStack;
    // 浏览栈：支持多层进入子电路后逐层返回
    QString m_currentInstancePath;
    // 当前实例路径："实例ID/实例ID/..."，用于从父实例同步子电路输入状态

    QVariantList m_clipboardCache;
    // 剪贴板缓存：copySelection 写入，pasteSelection 读取
    mutable QHash<QString, QStringList> m_ctxSubInstCache;
    // 上下文→子电路实例 ID 列表缓存：engineQidsFor 频繁查询，mutable 允许 const 函数更新
    bool         m_needFullApply = true;
    // 下次 apply 是否需要全量刷新：rebuild 后首次 apply 必须全量

    bool m_clockRunning = false;
    // 时钟是否运行
    int m_clockFrequency = 2;
    // 时钟频率（Hz）
    int m_clockTickCount = 0;
    // 时钟跳变计数
    QTimer* m_clockTimer = nullptr;
    // 时钟定时器：由 QObject 父子关系管理生命周期
    QHash<QString, bool> m_clockStates;
    // 每个 clock 元件的当前输出状态：key 为元件 ID

    QString m_engineType = "event";
    // 当前引擎类型：默认事件驱动
    bool m_simulationRunning = false;
    // 仿真是否运行中
    bool m_engineDirty = true;
    // 引擎是否需要重建：结构变化后置 true，下次仿真前 rebuild
    EventEngine* m_eventEngine = nullptr;
    // 事件引擎实例：析构函数中手动 delete（非 QObject 子对象）
    QTimer* m_simTimer = nullptr;
    // 仿真定时器：16ms 一帧，驱动 onSimTimerTick
    qulonglong m_simulationSpeed = 1000;
    // 每帧推进的仿真时间单位
    int m_simulationSpeedExp = 3;
    // 速度的 10 的指数形式：与 m_simulationSpeed 同步
    bool m_autoSpeed = true;
    // 自动速度标志
    bool m_showSimTime = false;
    // 是否显示仿真时间

    QString m_wireMode = "bezier";
    // 连线全局模式：bezier/ortho/switchable
    QString m_currentWireType = "bezier";
    // switchable 模式下当前线型

    CircuitUndoData* und() const;
    // 从动态属性中取出撤销数据指针：用 void* 包装避免头文件暴露完整类型
    Context& currentCtx();
    // 返回当前上下文引用：根或 m_subContexts 中对应项，找不到回退根
    const Context& currentCtx() const;
    // const 版本
    Context* ctxById(const QString& id);
    // 按 ID 查上下文指针：空串返回 &m_root，找不到返回 nullptr
    const Context* ctxById(const QString& id) const;
    // const 版本
    int indexOfComponent(const QString& id) const;
    // 在当前上下文中查找元件索引：线性扫描，委托 Context::indexOfComponent
    int indexOfWire(const QString& id) const;
    // 在当前上下文中查找连线索引

    void evaluateAll();
    // 全量求值：同步子电路端口、按引擎类型迭代或事件求值、最后单独求值当前子电路
    void loadRecentFiles();
    // 从 QSettings 读取最近文件：委托 RecentStore::load
    void saveRecentFiles();
    // 写回 QSettings：委托 RecentStore::save
    void loadSettings();
    // 读取引擎/速度/时钟/连线模式等全局设置：带默认值与钳位
    void saveSettings();
    // 写回全局设置到 QSettings
    bool hasSubCycle(const QString& fromCtx, const QString& targetId, QSet<QString>& visited) const;
    // 深度优先检测子电路引用环：用于 canAddSubInstance

    void pushUndo(const QString& mergeKey = QString());
    // 推送当前上下文快照：默认无合并键
    void pushUndoAll(const QString& mergeKey = QString());
    // 推送全量快照：涉及跨上下文操作（删除子电路、导入等）时使用
    void pushUndoMeta(const QString& mergeKey = QString());
    // 推送元信息快照：仅子电路名与计数器
    void pushUndoInternal(UndoScope scope, const QString& ctxId, const QString& mergeKey);
    // 统一入栈实现：合并键 + 500ms 窗口去重，超限裁剪
    UndoEntry captureUndoEntry(UndoScope scope, const QString& ctxId, const QString& mergeKey) const;
    // 按 scope 捕获快照内容
    UndoEntry captureSameScopeAs(const UndoEntry& ref) const;
    // 以参考条目的 scope 捕获当前状态：undo/redo 时保存反向快照用
    void applyUndoEntry(const UndoEntry& e);
    // 应用快照：恢复数据后重建引擎并全量刷新

    void rebuildEventEngine();
    // 重建事件引擎：收集所有上下文元件/连线，生成虚拟线，调用 EventEngine::build
    void applyEngineOutputsToContexts();
    // 把引擎输出写回上下文：支持增量（脏节点）与全量两种模式
    void onSimTimerTick();
    // 仿真定时器回调：必要时 rebuild、按速度推进、apply、发 tick 信号
    void runEventEngineFully(quint64 advance, quint64 budget);
    // 反复 run 直到事件队列空闲：解决大电路一次 run 收敛不了的问题
    QStringList engineQidsFor(const QString& ctxId, const QString& localCompId) const;
    // 把本地元件 ID 映射为引擎中的 qualified ID：子电路元件需加实例路径前缀
    void syncSubStateFromParent(const QString& subId);
    // 从父实例把输入值同步到子电路上下文的 input 元件：浏览时保持状态一致
    void refreshCurrentSubContext();
    // 刷新当前子电路上下文：同步父状态后单独求值
    void removeSubContextsByPrefix(const QString& prefix);
    // 按前缀批量删除子电路上下文及其在所有上下文中的实例

    //移动元件时更新连接到该元件的 ortho wire path
    void updateWiresForMovedComponent(Context& ctx, const QString& movedId);
    // 移动元件后修正其连接的 ortho 线首/尾点及相邻拐点，保持正交

    static int componentInputCount(const QVariantMap& comp);
    // 静态转发：ComponentLogic::inputCount
    static int componentOutputCount(const QVariantMap& comp);
    // 静态转发：ComponentLogic::outputCount
    static int outputPortBitWidth(const QVariantMap& comp, int portIdx);
    // 静态转发：ComponentLogic::outputBitWidth
    static int inputPortBitWidth(const QVariantMap& comp, int portIdx);
    // 静态转发：ComponentLogic::inputBitWidth
    void dumpInternalState(const QString& where) const;
    // 调试辅助：当前为空实现，保留用于打印内部状态
};

#endif
// 结束头文件保护