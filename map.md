# LogicSim 项目文件结构说明

> 本文档按目录顺序详细说明 LogicSim（Qt6 + QML 数字电路仿真器）中每个文件和文件夹的职责。

---

## 一、项目根目录

### `CMakeLists.txt`
项目的构建脚本。主要内容：
- 要求 CMake 3.16+、C++17、Qt 6.5+（Quick / QuickControls2 / Qml）。
- 定义三组源文件清单：
  - `CIRCUIT_SOURCES`：所有 C++ 实现文件（Circuit 各分部、core、logic、io、move 等）。
  - `CIRCUIT_HEADERS`：对应头文件（同时作为 SOURCES 参与 moc 处理）。
  - `QML_SOURCES`：所有 QML 组件（主窗口、主题、各栏、画布、面板、弹窗）。
- 把 `qml/Theme.qml` 标记为 QML 单例类型。
- `qt_add_executable` 生成可执行文件 `LogicSim`。
- `qt_add_qml_module` 把 QML 文件、JS 资源（Geometry.js / Painter.js / CanvasInteraction.js）打包进 `LogicSim` 模块。
- 链接 `Qt6::Quick`、`Qt6::QuickControls2`、`Qt6::Qml`。
- Android 平台下指定 `android/` 作为打包资源目录。

### `main.cpp`
程序入口：
- Android 下设置 RHI 后端为 OpenGL。
- 设置组织名/应用名（供 QSettings 使用）。
- 设置 Quick Controls 样式为 `Basic`（外观完全由 QML Theme 控制）。
- 用 `qmlRegisterType<Circuit>` 把 C++ 的 `Circuit` 注册为 QML 类型 `LogicSim.Circuit`。
- 创建 `QQmlApplicationEngine`，加载 `LogicSim` 模块的 `Main` 组件，进入事件循环。

---

## 二、`circuit/` —— C++ 电路模型层

### 2.1 `circuit/Circuit.h` / `circuit/Circuit.cpp`
- **Circuit.h**：`Circuit` 类的完整声明，继承 `QObject`，是整个应用的模型核心。暴露大量 `Q_PROPERTY`（它的核心作用是让普通的 C++ 成员变量（或读写函数）能够被 Qt 的元对象系统识别和访问）（components、wires、contextId、subcircuits、errors、engineType、simTime、gateDelay 等）与 `Q_INVOKABLE` （用于将普通的 C++ 成员函数注册到元对象系统中，使其能够被反射机制（如 QMetaObject::invokeMethod()）或 QML 调用。
）方法（增删元件/连线、移动、撤销重做、保存加载、子电路进出、仿真控制等）。定义了信号（changed、structureChanged、contextChanged、clockChanged、simulationTick 等）与私有成员（根上下文、子电路表、时钟状态、引擎指针、定时器、撤销数据等）。
- **Circuit.cpp**：构造/析构、全局设置读写（loadSettings/saveSettings）、撤销数据指针存取、canUndo/canRedo、上下文访问（currentCtx/ctxById）、indexOfComponent/Wire、仿真时间与门延迟 getter/setter、速度设置、线型模式设置等基础实现。

### 2.2 `circuit/core/` —— 基础数据结构与工具

| 文件 | 作用 |
|---|---|
| `CircuitContext.h` | `CircuitContext` 结构体：一组元件（QVariantList）+ 连线（QVariantList）+ 两个 ID 计数器，提供 `indexOfComponent`/`indexOfWire`/`isEmpty`。整个工程的"上下文"单位（根电路或子电路）。 |
| `CircuitContext.cpp` | 实现上述两个线性查找函数。 |
| `Component.h` | 元件 QVariantMap 的字段存取器命名空间：`id`/`type`/`name`/`bitWidth`/`rotation` 等 getter，以及 `isInput`/`isGate`/`isSub` 等类型判定、`qualifiedId` 拼接。 |
| `Wire.h` | 连线 QVariantMap 的字段存取器：`id`/`fromComp`/`toComp`/`fromPort`/`toPort`/`connects`。 |
| `IdAllocator.h/.cpp` | ID 发号器：`nextComponentId` 生成 "C<n>"，`nextWireId` 生成 "W<n>"，按上下文独立自增。 |
| `UndoData.h` | 撤销系统数据结构：`UndoScope` 枚举（CurrentCtx/Full/MetaOnly）、`UndoEntry`（快照条目）、`CircuitUndoData`（撤销/重做栈 + 合并键 + 时间戳 + 标志位 + 上限）。 |
| `ValueCodec.h/.cpp` | 二进制串与多进制互转：`bitsToString`、`bitAt`、`format`（bin/hex/udec/sdec/f32/f64）、`parse`（反向解析，含范围校验与错误消息）。 |

### 2.3 `circuit/logic/` —— 元件与连线逻辑

| 文件 | 作用 |
|---|---|
| `ComponentLogic.h/.cpp` | **元件知识的唯一入口**：`inputCount`/`outputCount`/`outputBitWidth`/`inputBitWidth`（按类型/端口算数量与位宽）、`outputPorts`/`inputPorts`（按类型计算端口几何并应用旋转）、`computeCombinationalOutputs`（not/and/or/nand/nor/xor/xnor/tgate/ntran/ptran/splitter/hub 的组合逻辑与三态语义，两套引擎共用）。 |
| `WireNetworkSolver.h/.cpp` | 连线网络求解：`generateVirtualWires` 用并查集把电气连通的位置/端口合并，输出"输出端口→输入端口"的虚拟线（供引擎建图，处理 T 型接入、十字交叉不连通等规则）；`connectionDots` 计算连接圆点（连接数 ≥ 3 的位置）。 |
| `SimEngine.h` | 引擎种类枚举 `EngineKind{Event,Iter}` 与字符串互转的 inline 函数。 |

### 2.4 `circuit/logic/event/` —— 事件驱动引擎

| 文件 | 作用 |
|---|---|
| `EventEngine.h` | `EventEngine` 类声明：节点 `Node`、事件 `Event`（Recalc/Wire）、驱动表、输出目标表、端口驱动值表、优先队列、脏节点追踪、门延迟覆盖表、全部公有接口（build/clear/reset/trigger*/step/run/drainNow/getOutputs/getInputs/takeDirtyQids 等）。 |
| `EventEngine.cpp` | 构造/析构、延迟参数 getter/setter、`clear`（完全清空）、`reset`（保留输入值软重置）。 |
| `EventEngine_build.cpp` | `build` 入口 + `buildRecursive`（递归展开子电路建节点，处理 sub 实例端口映射到内部 input/output 节点）+ `initValues`（初始化初值）+ `initDriverValues`（建立多驱合并表并做 32 轮预传播、调度初始 Recalc）。 |
| `EventEngine_run.cpp` | 核心运行：`resolveMultiDriver`（多驱按位合并，0/1 冲突→E）、`mapUnary`（逐位取反）、`scheduleRecalc`/`scheduleWirePropagation`（入队）、`step`/`drainNow`/`run`（推进与预算控制）、`processEvent`（分派 Recalc/Wire）、`recalcNode`（重算、标脏、按门延迟调度传播）、`computeOutputs`（数据源自行处理，组合逻辑委托 ComponentLogic）。 |
| `EventEngine_trigger.cpp` | 外部触发接口：`triggerInputBit`/`triggerInputString`/`triggerClockToggle`/`triggerForceOutput`（改值并调度下游传播），以及 `getOutput`/`getOutputs`/`getInputs` 查询。 |

### 2.5 `circuit/logic/iter/` —— 迭代求值引擎

| 文件 | 作用 |
|---|---|
| `IterEngine.h` | `IterEngine` 类声明，全 static 方法：`evalOne`/`evaluateContext`/`syncSubPins`/`evaluateAll`/`evaluateStandalone`。 |
| `IterEngine.cpp` | 实现：`evalOne` 收集输入、按类型算输出、仅变化时写回；`evaluateContext` 生成虚拟线、建图、拓扑排序求值（有环时迭代 200 轮）；`syncSubPins` 同步 sub 实例端口元数据与端口值数组；`evalInstance`/`evalRecursive` 递归求值子电路；`evaluateAll`/`evaluateStandalone` 入口。 |

### 2.6 `circuit/logic/move/` —— 移动元件时的自动重连线

| 文件 | 作用 |
|---|---|
| `MoveRequest.h` | 移动请求值对象：手势指针 + dx/dy，提供 `equals`/`hash`（作为缓存 key）。 |
| `MoveResult.h` | 计算结果：`additions`（新增线）、`removals`（删除线）、`replacements`（替换线对）、`unsatisfied`（未满足连接）、`totalDistance`。 |
| `ConnectionData.h` | 一条连接的描述：端口位置、方向、沿端口外走的 wire 链、链末端坐标，提供 `hash`（作缓存 key）。 |
| `AvoidanceMap.h/.cpp` | A* 寻路网格占用图：稀疏哈希表记录每个格子的允许方向（neither/vert/horz）；`create` 工厂批量标记元件包围盒与 ortho 线，`markAll`/`markComponent`/`markWire`/`unmarkWire`/`unmarkLocation`/`cloneMap`/`get`。 |
| `SearchNode.h/.cpp` | A* 搜索节点：位置、方向、目标、父指针、累计距离、启发值；`next` 扩展邻居（含延伸优惠、穿越惩罚、转弯惩罚），`getHeuristic` 计算启发值（含回头路惩罚），`hash`/`equals` 状态去重。 |
| `Connector.h/.cpp` | 重连计算器：`computeWires` 入口——从手势取连接描述、剔除不可达连接、为每条连接生成 A* 起点、多次尝试不同连接顺序（排序/反序/随机）、保留 unsatisfied 最少+总距离最短的方案；内部辅助 `findShortestPath`/`convertToPath`/`processPath`/`processConnection`/`tryList`。 |
| `ConnectorThread.h/.cpp` | 单例工作线程：请求队列 + 互斥量 + 条件变量，`enqueueRequest`（优先/普通入队）、`isOverrideRequested`（供 A* 中断过期计算）、`run` 循环取请求调用 Connector 并回调 `gesture->notifyResult`。 |
| `MoveGesture.h/.cpp` | 一次移动手势会话：`getConnections`（懒计算：收集选中元件端口、沿非选中 ortho 线向外走、记录 wire 链与方向）、`getFixedAvoidanceMap`（懒创建：只含非选中元件与非选中 ortho 线）、`notifyResult`/`findResult`/`enqueueRequest`/`forceRequest`（结果缓存与提交）。 |

### 2.7 `circuit/io/` —— 序列化、剪贴板、最近文件

| 文件 | 作用 |
|---|---|
| `JsonCodec.h/.cpp` | 存档编解码：`encode` 把根上下文/子电路/名称/计数器/时钟/引擎参数/速度/门延迟覆盖打包成 QVariantMap（含 version=11）；`decode` 反解析为 `DecodedData`，做范围钳位并调用 `fixCtx`（补全缺失字段、规范化 inputBits、补 outputSplits、收集时钟状态）。 |
| `ClipboardCodec.h/.cpp` | 复制/粘贴编解码：`encode` 从上下文抽取选中元件及相关连线（只选线→只复制线；选了元件→直接相连线 + 迭代扩散端点重合/T 型接入的线）；`paste` 分配新 ID、平移坐标、重写连线两端（ortho 允许一端悬空，bezier 要求两端成功）、返回新 ID 与新增时钟状态。 |
| `RecentStore.h/.cpp` | 最近文件持久化：用 QSettings 存一段 JSON 数组字符串，`load`/`save` 带解析容错。 |

### 2.8 `circuit/app/` —— Circuit 类的分部实现

| 文件 | 作用 |
|---|---|
| `Circuit_catalog.cpp` | 四个静态转发函数：`componentInputCount`/`componentOutputCount`/`outputPortBitWidth`/`inputPortBitWidth`，委托 `ComponentLogic`。 |
| `Circuit_component.cpp` | 元件相关操作：`addComponent`（按类型初始化字段/端口，钳位位宽与输入数，压撤销，插上下文，求值发信号）、`removeComponent`/`removeComponents`（删元件并清理关联线含 T 型线）、`moveComponent`/`moveSelection`（移动并随动 ortho 线，`updateWiresForMovedComponent` 修正首末点与相邻拐点）、`toggleInput`/`setBitValue`（改输入值，仿真中走增量触发）、`setComponentProp`（位宽/输入数/旋转的结构化修改）、`rotateComponent`/`renameComponent`/`setSplitterSplits`/`getSplitterSplitsStr`/`addText`/`setTextContent`。 |
| `Circuit_wire.cpp` | 连线相关：`findPortAt`（端口命中）、`allWireSegments`（枚举正交线段）、`connectionDots`、`addOrthoWire`（吸附端口、反向交换、生成 L 型 path）、`shortenWireEnd`（缩短一端并插拐点）、`addWire`（bezier）、`removeWire`、`wireAtPoint`（ortho 线段距离 / bezier 采样距离命中）、`setWirePathPoint`（拖拐点并联动相邻点）、`getWirePath`/`getWireType`。 |
| `Circuit_subcircuit.cpp` | 子电路：`addSubcircuit`/`deleteSubcircuit`（删除并清理所有实例）/`setSubcircuitName`/`getSubcircuitName`/`hasSubCycle`（DFS 检测引用环）/`canAddSubInstance`/`refreshCurrentSubContext`/`enterSubcircuit`（压栈、算实例路径、同步父输入）/`leaveSubcircuit`（弹栈恢复）/`switchEditContext`/`setSubcircuitAsRoot`（与根互换）/`syncSubStateFromParent`（从父实例或引擎同步输入值到子电路 input 元件）。 |
| `Circuit_simulation.cpp` | 时钟与仿真：`tick`（翻转所有 clock 并触发引擎/迭代）、`startClock`/`stopClock`/`setClockFrequency`、`singleStep`（事件引擎 step 或 tick）、`toggleClock`（手动翻转）、`resetAllInputs`、`setGateTypeDelay`/`getGateTypeDelay`/`clearGateTypeDelays`、`startSimulation`/`pauseSimulation`/`resetSimulation`/`simulateStep`/`onSimTimerTick`（每帧按速度推进并 apply）。 |
| `Circuit_selection.cpp` | 复制/粘贴：`copySelection`（调 ClipboardCodec::encode 缓存）、`pasteSelection`（压撤销、调 paste、登记新时钟状态、求值发信号）。 |
| `Circuit_query.cpp` | 查询：`getComponent`/`getWire`/`outputPortsInfo`/`inputPortsInfo`/`allPortsInfo`/`getInputBase`/`setInputBase`/`formatValue`/`getInputAsString`/`setInputFromString`（解析文本写 inputBits，仿真中触发引擎）/`statistics`（递归统计各类元件数量，visited 防循环引用）。 |
| `Circuit_lifecycle.cpp` | 生命周期与文档：`subcircuits`/`editContexts`（列表查询）、`setEngineType`（切引擎：暂停、停钟、重建、全量刷新）、`evaluateAll`/`evaluate`（核心求值：syncSubPins、按引擎类型求值、浏览模式单独求值当前子电路）、`rebuildEventEngine`（拍平上下文树建引擎）、`runEventEngineFully`（反复 run 到空闲）、`applyEngineOutputsToContexts`（全量/增量回写，`updateOneComponent` 按类型写字段）、`engineQidsFor`（本地 ID→限定 ID 列表）、`clear`/`refreshErrors`（位宽不匹配检查）/`toJson`/`fromJson`/`saveToFile`/`loadFromFile`（含 `extractFirstJsonObject` 容错提取、`isOnlyWhitespaceFrom` 判断回写修复）/`removeSubContextsByPrefix`/`importFromFile`（前缀化导入，处理重复导入/残留清理）/`removeImportedFile`/`importedFiles`、撤销系统（`captureUndoEntry`/`captureSameScopeAs`/`applyUndoEntry`/`pushUndoInternal`/`pushUndo`/`pushUndoAll`/`pushUndoMeta`/`breakUndoMerge`/`undo`/`redo`）、最近文件（`loadRecentFiles`/`saveRecentFiles`/`addRecentFile`/`removeRecentFile`/`clearRecentFiles`）。 |

---

## 三、`qml/` —— QML 前端

### 3.1 `qml/Main.qml`
应用主窗口 `ApplicationWindow`：
- 创建 `Circuit` 模型对象，暴露为 `circuitRef`。
- 维护大量视图状态：缩放/偏移、放置/拉线状态、选中状态（单选/多选/子电路/连线）、框选、拖动、脏标记、剪贴板、子电路/上下文/最近文件/错误列表缓存、各元件端口几何缓存等。
- 连接 Circuit 各信号到缓存刷新/重绘/错误防抖。
- 定义快捷键（撤销/重做/删除/复制/粘贴/返回）。
- 定义缓存函数（`updateCachesValues`/`rebuildCachesFull`/`refreshSel`）、视图函数（`resetView`/`zoomIn`/`zoomOut`/`triggerRepaint`）、文件函数（`fileUrlToLocal`/`doSave`/`doSaveAs`/`doOpenFile`/`doExit`/`quitApp`）、删除确认等。
- 布局：顶部区（TopBar + ClockBar 或收起条）、只读提示条、左侧面板区（LeftPanel 或收起条）、画布区（CanvasView + StatusBar）、设置页、各对话框/弹窗实例、两个 FileDialog。

### 3.2 `qml/Theme.qml`
`pragma Singleton` 主题单例：集中定义颜色（背景/边框/文字/强调色/连线色）、尺寸（各栏高度、面板宽、圆角、收起条）、字号（fsTiny~fsLarge）。整个 QML 通过 `Theme.xxx` 引用。

### 3.3 `qml/views/bars/` —— 工具条

| 文件 | 作用 |
|---|---|
| `TopBar.qml` | 顶部工具栏：标题、菜单按钮、模式切换（编辑/控制/选择）、线型切换（可切换模式下）、撤销/重做、缩放（缩小/百分比重置/放大）、上下文切换按钮、脏标记、收起按钮。暴露 `ctxBtn`/`menuBtn` 供弹窗锚点。 |
| `ClockBar.qml` | 时钟工具条：仿真开始/暂停/单步/复位（事件引擎）、仿真时间显示、时钟启动/停止/时钟拍/复位输入、频率 SpinBox、拍数显示。 |
| `StatusBar.qml` | 底部状态栏：显示 `[模式] 状态文本`，有错误时用红色。 |

### 3.4 `qml/views/canvas/` —— 画布

| 文件 | 作用 |
|---|---|
| `CanvasView.qml` | 画布视图：`Canvas` 元素 + `paintCanvas` 绘制入口（网格、连线、圆点、拉线预览、元件）；`compSize` 计算元件显示尺寸（带签名缓存）；坐标换算 `worldToScreen`/`screenToWorld`/`portPos`/`bitIndexAt`；`computeWireSources` 递归追上游给连线着色；`computeNearestAngle`/`jumpToNearest` 导航指示器；框选框、导航指示器、`MouseArea` 转发事件到交互 JS；暴露 `requestPaint`/`canvasWidth`/`canvasHeight`。 |
| `Geometry.js` | 共享 JS 库：`worldToScreen`/`screenToWorld`/`isVisible`（包围盒可见性）/`roundRect`（圆角矩形路径）。 |
| `Painter.js` | 共享 JS 库：`paintGrid`（网格）、`pickWireStyle`/`pickWireColor`（按信号选线色/虚线）、`drawWire` 分派、`drawWireBezier`/`drawWireOrtho`（按 wireSources 着色、选中高亮）、`drawConnectionDots`、`drawPendingBezier`/`drawPendingOrtho`（拉线预览）、`classifyPortStr`/`fillPortColor`（端口状态色）、`drawComponent`（按类型绘制所有元件形状/文字/端口/名称/端口标注）。 |

### 3.5 `qml/views/canvas/interactions/CanvasInteraction.js`
画布交互逻辑：
- `findPortAt`（屏幕坐标附近找最近端口）、`resolveEffectiveWireType`、`snapWorldPoint`（吸附端口或 10px 网格）、`snapToWire`（吸附到 ortho 线最近点并网格化）、`updateWiringDirection`（拐角方向切换）、`pointOnSegInterior`、`findShortenTargetAt`（起点处可缩短的线）、`startWiringFromPort`/`startWiringFromWire`。
- `onPressed`（放置元件、select 模式命中线/元件/框选、编辑模式端口优先拉线/线中间 T 型引出/命中元件/空白平移）、`onPositionChanged`（框选/拖动选中集合/拉线预览/拖单个元件/平移）、`onReleased`（框选收集、拉线结束：缩短/创建 bezier 或 ortho/放弃）、`onCanceled`（清理状态）、`onWheel`（以鼠标为锚点缩放）、`onDoubleClicked`（双击子电路实例进入浏览）。

### 3.6 `qml/views/panels/` —— 面板

| 文件 | 作用 |
|---|---|
| `LeftPanel.qml` | 左侧面板：元件库（按分组展示所有可放置元件 + 子电路列表，支持展开/收起、点击进入放置态、子电路右键/长按菜单：选中/重命名/设为主电路/删除、新建子电路按钮）+ 属性面板；含重命名子电路对话框。暴露各输入框 alias。 |
| `PropertyPanel.qml` | 属性面板：按选中对象显示——多选操作（复制/粘贴/删除选中）、子电路属性（SubcircuitPanel）、连线属性（ID/类型/删除）、元件属性（ID/类型/名称/方向/位宽/输入数/门延迟覆盖/显示进制 ComboBox/LED 颜色/非二进制值编辑/二进制逐位开关/分线器分段/文本内容/晶体管端口说明/删除按钮）、错误列表。 |
| `SubcircuitPanel.qml` | 子电路属性子面板：名称编辑、ID 与端口数信息、重命名/设为主电路/进入浏览/删除按钮。 |
| `props/LedProps.qml` | LED 颜色选择色块行。 |
| `props/SplitterProps.qml` | 分线器/集线器分段文本编辑框（逗号分隔，和=位宽）。 |
| `props/TextProps.qml` | 文本元件内容编辑框。 |

### 3.7 `qml/views/dialogs/` —— 对话框与弹窗

| 文件 | 作用 |
|---|---|
| `ConfirmDialog.qml` | 通用确认框：标题 + 消息 + 取消/确定（确定执行 `action` 回调）。 |
| `CloseConfirmDialog.qml` | 关闭（退出）时有未保存修改：取消/不保存/保存（保存后退出，失败转另存为）。 |
| `ExitConfirmDialog.qml` | 无未保存修改时的退出确认：取消/退出。 |
| `UnsavedDialog.qml` | 执行打开/新建前有未保存修改：取消/不保存（执行 `pendingAction`）/保存（有路径直接存，无路径转另存为并把回调交给 `pendingAfterSave`）。 |
| `SubNameDialog.qml` | 新建子电路：输入名称，回车或点创建调用 `addSubcircuit`。 |
| `ContextPopup.qml` | 上下文选择弹窗：列出根 + 所有子电路，点击 `switchEditContext`。 |
| `MenuPopup.qml` | 主菜单：保存/另存为/打开/导入…/已导入…/最近/统计/新建/设置，内部组件 `MenuRow`。 |
| `RecentPopup.qml` | 最近打开文件：列出条目（名称/时间/路径）、点击打开（含委托重建后引用失效的闭包捕获修复）、单条删除、清空。 |
| `StatsPopup.qml` | 电路统计：用 ListModel 显示各类元件数量（递归含子电路）。 |
| `ImportedFilesPopup.qml` | 已导入文件：列出（基名/电路数/路径）、取消导入按钮。 |
| `SettingsPage.qml` | 全屏设置页：仿真引擎选择、连线模式选择、传播延迟（门/导线，仅事件引擎）、仿真速度（自动/指数/显示仿真时间）、应用并返回/恢复默认；`syncFromCircuit`/`apply`/`resetToDefaults` 函数。 |

---

## 四、`android/` —— Android 打包资源

| 路径 | 作用 |
|---|---|
| `res/mipmap-*/ic_launcher.png` | 各密度启动图标（二进制）。 |
| `AndroidManifest.xml` | Android 清单：应用名 LogicSim、QtApplication/QtActivity、横屏、单顶启动、MAIN/LAUNCHER intent、lib_name=LogicSim、INTERNET 权限。 |

---

## 五、整体架构速览

```
main.cpp ──注册──> Circuit (C++ 模型)
                      │
        ┌─────────────┼──────────────────────────────┐
        │             │                              │
   core/          logic/                          io/
 (上下文/编解码/  (ComponentLogic 元件知识、       (JsonCodec 存档、
  撤销数据/ID)    WireNetworkSolver 拓扑、         ClipboardCodec 复制粘贴、
                  EventEngine 事件引擎、           RecentStore 最近文件)
                  IterEngine 迭代引擎、
                  move/ 移动重连线)
                      │
              app/Circuit_*.cpp (分部实现)
                      │
                      ▼
              QML 前端 (qml/)
   Main.qml ──> Circuit 对象 (circuitRef)
      │
      ├─ views/bars/    顶栏/时钟栏/状态栏
      ├─ views/canvas/  CanvasView + Geometry/Painter/CanvasInteraction
      ├─ views/panels/  左面板/属性面板/各 props 子面板
      └─ views/dialogs/ 各种对话框与弹窗
```

- **数据层**：元件与连线统一用 `QVariantMap` 表示，上下文用 `CircuitContext`，全部可值语义拷贝（撤销/导入/粘贴依赖此特性）。
- **逻辑层**：`ComponentLogic` 是元件规则的唯一事实来源；`WireNetworkSolver` 负责把几何连线转成电气网络；两套引擎（事件/迭代）都基于前两者，保证行为一致。
- **模型层**：`Circuit` 聚合所有状态，通过信号与 `Q_INVOKABLE` 方法对 QML 暴露。
- **视图层**：QML 通过 `circuitRef` 读写模型，Canvas 自绘全部图形，交互逻辑集中在 `CanvasInteraction.js`。
- **移动重连**：`move/` 子模块用 A* 在避让网格上为被移动元件重新规划连线，运行在独立线程 `ConnectorThread` 中，通过 `MoveGesture` 缓存结果。
