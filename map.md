# LogicSim 项目文件结构说明

> 本文按当前代码说明 LogicSim（Qt 6.5+、C++17、QML 数字电路仿真器）的目录、主要文件职责和阅读路线。项目已采用分层静态库、C++/QML 元件双注册和整数正交拓扑；描述以仓库现状为准，不把特定基准场景的测量当作普遍性能结论。

## 目录

- [零、总览](#零总览)
- [一、项目根目录与构建](#一项目根目录与构建)
- [二、`src/core/`：基础数据与整数网格](#二srccore基础数据与整数网格)
- [三、`src/components/`：C++ 元件定义](#三srccomponentsc-元件定义)
- [四、`src/logic/`：元件门面与连线拓扑](#四srclogic元件门面与连线拓扑)
- [五、`src/io/`：序列化与持久化](#五srcio序列化与持久化)
- [六、`src/engine/`：仿真引擎](#六srcengine仿真引擎)
- [七、`src/move/`：移动重连线](#七srcmove移动重连线)
- [八、`src/model/`：Circuit 模型](#八srcmodelcircuit-模型)
- [九、`src/app/`：程序入口](#九srcapp程序入口)
- [十、`qml/`：界面与元件 QML 注册](#十qml界面与元件-qml-注册)
- [十一、`tests/`：自动测试与独立性能 harness](#十一tests自动测试与独立性能-harness)
- [十二、`docs/` 与 `android/`](#十二docs-与-android)
- [十三、依赖关系、构建与测试](#十三依赖关系构建与测试)
- [十四、阅读路线](#十四阅读路线)

## 零、总览

```text
CMakeLists.txt
├── src/core/       logicsim_core：数据、撤销、值编解码、Grid
├── src/logic/      logicsim_logic：逻辑门面、拓扑求解、builtin factory
├── src/components/ C++ Definition 基类与每类型独立定义
├── src/io/         logicsim_io：JSON、剪贴板、最近文件
├── src/engine/     logicsim_engine：事件引擎与迭代引擎
├── src/move/       logicsim_move：正交连线移动规划
├── src/model/      logicsim_model：Circuit QObject 与分部实现
├── src/app/main.cpp
├── qml/            LogicSim QML 模块、画布、面板、dialogs、元件定义
├── tests/          CTest 单元/模型/集成用例；perf 子项目独立配置
├── docs/           架构说明
└── android/        Android 打包资源
```

运行数据主路径：`qml/Main.qml` 调用 `Circuit`；`Circuit` 持有上下文并协调 `ComponentLogic`、`WireNetworkSolver`、引擎及 IO；画布读取模型快照绘制。内置类型由 `src/components/builtin/*.cpp` 提供语义和端口几何，同时由 `qml/components/builtin/*Definition.qml` 提供外观、库元数据与属性编辑器。

## 一、项目根目录与构建

### `CMakeLists.txt`

顶层配置要求 Qt 6.5+，设定 C++17/AUTOMOC，添加 core、logic、io、move、engine、model 子目录，并在顶层创建 `LogicSim` 可执行目标和 `LogicSim` URI 的 QML 模块。`LogicSim` 链接 model 与 Qt Quick；测试由 `enable_testing()` 和 `add_subdirectory(tests)` 注册。`src/logic/CMakeLists.txt` 扫描 builtin C++ 文件并生成 `BuiltinFactories.inc`，避免仅靠静态初始化注册时实现被静态链接裁剪。顶层 CMake 扫描 QML 定义，生成构建目录中的 `DefinitionManifest.js`，并作为资源嵌入；新增定义后重新配置即可发现。

### 其他根文件

- `README.md`：项目介绍、功能、构建和使用说明。
- `LICENSE`：GPLv3。
- `map.md`：本导航文档。
- `qtWin.sh`：MXE Windows 交叉编译脚本，需配置对应工具链。
- `合并.sh`：维护者辅助脚本，不参与应用构建。

## 二、`src/core/`：基础数据与整数网格

只依赖 Qt Core，为其他层提供共享数据约定。

- `CircuitContext.h/.cpp`：一个根或子电路的组件、连线和 ID 计数器；根上下文以空 ID 表示。
- `Component.h`、`Wire.h`：`QVariantMap` 字段访问、类型判断和连线端点约定。
- `IdAllocator.h/.cpp`：上下文内组件/连线 ID 分配。
- `UndoData.h`：撤销/重做快照、范围和合并状态。
- `ValueCodec.h/.cpp`：二进制值与 bin/hex/有符号、无符号十进制及浮点显示值互转。
- `Grid.h`：坐标边界检查、整数化、10px 自由吸附、正交路径规范化/裁剪/投影，以及点在线段、T 型内点判定。持久化的元件坐标和正交路径用整数；拓扑相交采用精确行列判断。
- `CMakeLists.txt`：生成 `logicsim_core`，公开 `src/` include 根。

## 三、`src/components/`：C++ 元件定义

### `ComponentDefinition.h/.cpp`

`Components::Definition` 是每个内置类型的 C++ 契约：稳定 `typeId`、角色、输入/输出数量和位宽、端口局部几何、默认实例字段及纯组合计算。基类提供宽度、门端口布局、零值端口初始化、逐位逻辑规约和开关逻辑等共享机制；静态注册表持有每类型一个定义对象。`src/logic/CMakeLists.txt` 按文件名生成显式工厂引用，工厂名为 `create<文件名主体>Definition()`。

### `builtin/*.cpp`

当前共有 **19 个内置定义**，编入 `logicsim_logic`，**不是第七个库**：

| 文件（均在 `src/components/builtin/`） | 职责 |
|---|---|
| `And.cpp`、`Nand.cpp`、`Or.cpp`、`Nor.cpp`、`Xor.cpp`、`Xnor.cpp` | 可变输入数的逐位规约门及反相结果 |
| `Not.cpp` | 单输入逐位取反 |
| `Input.cpp`、`Const.cpp`、`Clock.cpp` | 输入、常量及时钟的角色、默认值和端口 |
| `Output.cpp`、`Led.cpp` | 输出观察与 LED 的语义和端口 |
| `Splitter.cpp`、`Hub.cpp` | 总线分段/汇合及各分段位宽 |
| `Tgate.cpp`、`Ntran.cpp`、`Ptran.cpp` | 传输门、NMOS、PMOS 的开关规则与非对称几何 |
| `Text.cpp`、`Sub.cpp` | 文本无电气端口；子电路实例端口随元数据生成 |

端口查询返回 `{index, x, y, bitWidth}`，`x/y` 是旋转后的局部偏移，调用方再加元件中心得到世界坐标。Tgate 逻辑输入 0 为数据、1 为使能；Ntran/Ptran 输入 0 为栅极、1 为数据。Tgate 保留三个历史存储输入值，但逻辑端口仍为两个。新增时序状态机不会自动获得新调度能力，组合 `compute` 应保持纯函数。新增类型流程和边界说明见 [`docs/component-registry.md`](docs/component-registry.md)。

## 四、`src/logic/`：元件门面与连线拓扑

- `ComponentLogic.h/.cpp`：兼容门面，将类型查询、默认值、几何、位宽和组合计算转发给 C++ Definition。端口局部几何统一按 rotation 四分之一圈变换并由 `Grid` 量化；两个引擎共享计算入口。
- `WireNetworkSolver.h/.cpp`：把正交线、端口和端点形成电气网络，输出仅供引擎建图的 virtual wires；T 型接入连通，纯中间段十字交叉不连通。整数二维坐标使用无碰撞的两个 32 位数打包键，线段按行/列索引候选再按原顺序检查。`connectionDots` 根据连接度计算画布接点圆点。
- `SimEngine.h`：事件/迭代引擎枚举及字符串转换。
- `CMakeLists.txt`：扫描 builtin C++ 文件、生成显式 factory include 并构建 `logicsim_logic`。

## 五、`src/io/`：序列化与持久化

- `JsonCodec.h/.cpp`：文档字段编解码、旧文档兼容和几何规范化。
- `ClipboardCodec.h/.cpp`：组件/连线复制粘贴、ID 重映射及偏移应用。
- `RecentStore.h/.cpp`：用 QSettings 存取最近文件。
- `CMakeLists.txt`：构建 `logicsim_io`，依赖 core、logic 和 Qt Core；迁移几何及端口绑定需要逻辑层的权威端口信息。

## 六、`src/engine/`：仿真引擎

两个引擎共享 `ComponentLogic` 组合语义及拓扑构图。

### `event/`

- `EventEngine.h/.cpp`：节点、事件优先队列、驱动映射、延迟参数、脏节点及查询接口。
- `EventEngine_build.cpp`：递归展开层级电路、建节点/连线图、初始化值和驱动。
- `EventEngine_run.cpp`：推进事件、逐位多驱合并、重算变化传播和预算控制。
- `EventEngine_trigger.cpp`：外部输入/时钟触发、强制输出和查询。

事件顺序、延迟和预算属于仿真语义；局部性能优化不应被理解为调度契约变化。

### `iter/`

`IterEngine.h/.cpp`：拓扑排序求值，环路迭代及子电路递归/端口同步；适合无时序组合求值。`CMakeLists.txt` 构建 `logicsim_engine`，链接 logic。

## 七、`src/move/`：移动重连线

独立的正交连线规划模块，依赖 logic 的端口几何。

- `AvoidanceMap.h/.cpp`：元件及现有正交线的稀疏占用图。
- `SearchNode.h/.cpp`：A* 状态、转弯/穿越代价及路径启发值。
- `ConnectionData.h`、`MoveRequest.h`、`MoveResult.h`：连接、异步请求和变更结果值对象。
- `MoveGesture.h/.cpp`：拖动会话、固定避让图和结果缓存。
- `Connector.h/.cpp`：连接顺序尝试、路径搜索及增删/替换连线结果。
- `ConnectorThread.h/.cpp`：请求队列与工作线程。
- `CMakeLists.txt`：构建 `logicsim_move`。

模型中的 `moveComponentWithWires` / `moveSelectionWithWires` 目前分别转调 `moveComponent` / `moveSelection`，不等于每次拖动都会启动 A*。当前交互手势用 `beginSelectionMove` 捕获起始上下文，`updateSelectionMove(totalDx,totalDy)` 每次从起点快照重建候选，并在整数边界检查通过后整体提交；`endSelectionMove` 结束会话，一次手势只压一次撤销。仅选正交线和混合选择也可移动；双端同动平移路径，单端随动保持正交，固定主干上的内部端口需保留原主干并处理分支。

`Circuit_wire.cpp` 的 `shortenWireEnd(wireId, shrinkFront, x, y)` 明确指定裁剪前端还是末端；目标先投影到原路径，再用 `Grid::clippedPath` 保留未裁掉部分的原拐点。它不是按端点距离猜测缩哪端，也不重新吸附到别的 10px 行列；裁剪后仅保留仍位于端点的显式端口绑定。对应交互在 `CanvasInteraction.js`，回归用例在 `tests/model/test_circuit_component.cpp` 和 `test_circuit_wire.cpp`。

## 八、`src/model/`：Circuit 模型

`Circuit` 是供 QML 使用的 QObject，持有根/子上下文、选择/文档状态、仿真参数和缓存，并协调 IO、拓扑和引擎。

- `Circuit.h`：Q_PROPERTY、Q_INVOKABLE、信号、状态成员及缓存声明。
- `Circuit.cpp`：生命周期、设置、基础访问器；构造时连接变更信号使圆点缓存失效，并持有渲染快照/接点缓存。
- `impl/Circuit_lifecycle.cpp`：求值和事件引擎重建/结果回写、文档加载保存、撤销重做、导入及最近文件。
- `impl/Circuit_component.cpp`：创建/删除/移动元件，默认字段委托 `ComponentLogic`；同步移动正交线、选择和输入操作。
- `impl/Circuit_wire.cpp`：建删线、路径编辑、命中、缩短和 `connectionDots()` 懒计算缓存。
- `impl/Circuit_subcircuit.cpp`：子电路创建、引用检查、浏览/编辑上下文及状态同步。
- `impl/Circuit_simulation.cpp`：时钟、仿真启停、单步及延迟设置。
- `impl/Circuit_selection.cpp`：复制粘贴委托 ClipboardCodec。
- `impl/Circuit_query.cpp`：对象/端口/值查询、格式化和统计。
- `impl/Circuit_catalog.cpp`：旧调用接口到 ComponentLogic 的薄转发。
- `CMakeLists.txt`：构建 `logicsim_model`，链接下层库。

### 快照与缓存

`renderSnapshot(knownRevision)` 为 QML 提供版本化的当前上下文组件和连线列表；调用方持有相同修订时只返回版本号。端口几何由 `allPortsInfo()` 单独查询，并由 QML 在全量刷新时缓存。模型通过容器共享存储身份判断快照是否需要更新。接点圆点由 `connectionDots()` 按需计算并缓存；编辑、几何、结构或上下文通知会使圆点缓存失效，普通重绘不必反复求拓扑。QML 在值变化时更新列表缓存，在几何/结构/上下文变化时重建端口与索引缓存。这些缓存只减少重复数据传递/计算，不改变拓扑结果。

## 九、`src/app/`：程序入口

`main.cpp` 创建 Qt GUI 应用、设置组织/应用名与 Basic 控件风格，注册 `Circuit` 类型，加载 `LogicSim.Main` QML 模块并运行事件循环。可执行 target 在根 `CMakeLists.txt` 创建，`src/app/` 没有独立 CMakeLists。

## 十、`qml/`：界面与元件 QML 注册

### 主界面与画布

- `Main.qml`：创建 Circuit，按模型信号维护渲染缓存、选择和视图状态，组合顶栏、面板、画布及弹窗；启动时调用 `ComponentRegistry.validate(circuit)` 校验 C++ 与 QML 类型集合一致。
- `Theme.qml`：QML singleton 主题。
- `views/canvas/CanvasView.qml`：坐标变换、渲染调度及 Canvas 绘制入口；使用模型快照和接点圆点。
- `views/canvas/Geometry.js`：世界/屏幕坐标及可见范围工具。
- `views/canvas/Painter.js`：网格、连线、端口状态及元件绘制调度；元件图形委托相应 QML Definition。
- `views/canvas/interactions/CanvasInteraction.js`：放置、命中、拉线、T 型接入、缩短、选择、拖动、缩放和子电路导航交互。

### 双注册元件目录

- `qml/components/ComponentDefinition.qml`：UI 定义基类，包含类型 ID、名称/分类/排序、尺寸、标签、绘制回调及可选 `propertyEditor`。
- `qml/components/ComponentRegistry.qml`：singleton，消费生成的清单并创建每种类型一个共享定义对象，提供类型查询、元件库分组和注册 parity 校验。
- `qml/components/DefinitionManifest.js`：源码树中的内置加载清单，便于直接从源码目录加载定义；正式构建不手工维护它作为资源清单，而由顶层 CMake 从 `qml/components/builtin/*.qml` 排序生成构建目录版本，以 `components/DefinitionManifest.js` 资源别名嵌入 QML 模块。
- `qml/components/builtin/*Definition.qml`：各类型 UI 外观、库元数据和属性编辑器。类型 ID 必须与 C++ 定义一致。

新增类型需成对添加 `src/components/builtin/Foo.cpp` 和 `qml/components/builtin/FooDefinition.qml`，遵循工厂命名并重新运行 CMake 配置。无需维护中央类型 switch；稳定持久化的 type ID 发布后不可随意改名。流程与限制见 [`docs/component-registry.md`](docs/component-registry.md)。

### 其余视图

- `views/bars/TopBar.qml`、`ClockBar.qml`、`StatusBar.qml`：编辑工具、仿真时钟及状态。
- `views/panels/LeftPanel.qml`、`PropertyPanel.qml`、`SubcircuitPanel.qml`：元件库、属性、子电路；`props/` 存放 LED、分线器/集线器和文本专用编辑器。
- `views/dialogs/ConfirmDialog.qml`：通用确认；`CloseConfirmDialog.qml` / `ExitConfirmDialog.qml`：有/无未保存修改时退出确认；`UnsavedDialog.qml`：打开/新建前的保存确认。
- `views/dialogs/SubNameDialog.qml` / `ContextPopup.qml`：新建子电路和切换编辑上下文。
- `views/dialogs/MenuPopup.qml`、`RecentPopup.qml`、`ImportedFilesPopup.qml`、`StatsPopup.qml`：主菜单、最近文档、导入记录和递归统计。
- `views/dialogs/SettingsPage.qml`：引擎、线型模式、传播延迟与仿真速度设置。

`LeftPanel` 从 `ComponentRegistry.libraryGroups` 获取普通元件库；`PropertyPanel` 根据定义能力及 `propertyEditor` 装载专用编辑器；`CanvasView.compSize` 和 `Painter.drawComponent` 从定义取得尺寸及绘制回调。前端不应另维护一套电气端口坐标，端口位置仍来自 C++ 查询。`Theme.qml` 与 `ComponentRegistry.qml` 均在顶层 CMake 标为 QML singleton。

## 十一、`tests/`：自动测试与独立性能 harness

### CTest 目标

`tests/CMakeLists.txt` 的 `logicsim_add_test` 为各用例生成单独 Qt Test 可执行文件，并注册到 CTest。当前 17 个目标：

- `core`：`test_grid`、`test_value_codec`、`test_circuit_context`、`test_id_allocator`
- `logic`：`test_component_logic`、`test_wire_network_solver`、`test_component_registry`
- `io`：`test_json_codec`、`test_clipboard_codec`
- `engine`：`test_iter_engine`、`test_event_engine`、`test_event_equivalence`
- `move`：`test_connector`
- `model`：`test_circuit_component`、`test_circuit_undo`、`test_circuit_document`、`test_circuit_wire`

共享夹具在 `tests/helpers/TestHelpers.h`、`EngineTestHelpers.h`。已有真实回归覆盖 Grid 规范化/投影、整数端口、T 接入/交叉、多驱、注册类型/默认值/非对称端口、IO 几何迁移、移动手势和裁剪绑定、圆点缓存失效。`test_event_equivalence` 检查多驱轨迹、位宽、脏顺序/reset、反馈预算和吞吐场景；它不是完整引擎语义或所有电路的等价证明。`test_circuit_context`、`test_id_allocator`、`test_iter_engine`、`test_event_engine`、`test_circuit_undo`、`test_circuit_document` 仍包含占位测试，目标存在或通过不等于功能全面覆盖。当前 CTest 不包含真实 GUI 交互/像素验收。

### `tests/perf/`

这是刻意不纳入应用顶层 CMake/CTest 的独立 benchmark 子项目。`CMakeLists.txt`、`benchmark.cpp` 和 `README.md` 定义 core/logic/engine 工作负载、指纹和独立构建方法；基准结果仅对指定 Qt、编译模式和工作负载有效。README 中记录了初始本地 Release 样本，但主机活动未受控，属于 smoke measurement，不能据此宣称通用性能提升，也不覆盖 GUI 帧耗时。

## 十二、`docs/` 与 `android/`

- [`docs/component-registry.md`](docs/component-registry.md)：双注册扩展步骤、历史兼容行为、优化边界、性能 harness 使用及验证限制。特别指出尚无完整 GUI/像素验收，特定多驱或拓扑工作负载不代表所有电路性能。
- `android/AndroidManifest.xml` 与 `android/res/mipmap-*/ic_launcher.png`：Android 应用配置和图标；由顶层 CMake 的 `QT_ANDROID_PACKAGE_SOURCE_DIR` 引用。

## 十三、依赖关系、构建与测试

### 依赖

```text
Qt6::Core -> logicsim_core -> logicsim_logic
                                  ├-> logicsim_io ─────┐
                                  ├-> logicsim_move ───┤
                                  └-> logicsim_engine ─┴-> logicsim_model -> LogicSim
Qt6::Quick / QuickControls2 / Qml ---------------------------------> LogicSim
Qt6::Test -> tests/*（每项链接所需库）
```

`src/components/` 的 C++ 实现由 `logicsim_logic` 编译，不是额外的静态库；QML Definition 随 LogicSim QML 模块编译。IO、move、engine 并列依赖 logic；model 协调三者。

### 桌面构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/path/to/Qt/6.5+/gcc_64
cmake --build build -j4
./build/LogicSim
```

具体可执行文件布局受平台/生成器影响；也可从 Qt Creator 打开根 `CMakeLists.txt` 并运行 `LogicSim` target。

### 测试

```bash
ctest --test-dir build --output-on-failure
```

也可按 CTest 名称过滤，例如 `ctest --test-dir build -R logic_test_component_registry --output-on-failure`。性能 harness 按 `tests/perf/README.md` 在 scratch 中单独 configure/build，不属于上述 CTest。

## 十四、阅读路线

1. 先读 `README.md` 和本文件，再看根 `CMakeLists.txt` 理解目标及自动生成步骤。
2. 从 `src/core/CircuitContext.h`、`Component.h`、`Wire.h` 和 `Grid.h` 建立数据/坐标模型。
3. 读 `src/components/ComponentDefinition.h` 与两三个 builtin 定义，再读 `src/logic/ComponentLogic.cpp`；需要新增类型时配合 `docs/component-registry.md`。
4. 读 `src/logic/WireNetworkSolver.cpp`，重点理解精确整数 T 接入和纯交叉的区别。
5. 先看 `src/engine/iter/IterEngine.cpp` 的组合求值，再看 event 的 build、run、trigger 三个文件理解延迟调度。
6. 以 `src/model/Circuit.h` 和 `impl/Circuit_lifecycle.cpp` 为入口，按任务转到 component、wire、subcircuit、simulation 分部。
7. 看 `qml/Main.qml` 的快照缓存与信号，再读 CanvasView、Painter、CanvasInteraction；元件 UI 从 ComponentRegistry 和对应 Definition.qml 入手。
8. 需要核实行为时找相应 `tests/` 用例；需要性能数据时先读独立的 `tests/perf/README.md` 及其限制。

理解系统时注意：根上下文 ID 是空串；几何持久化与拓扑以整数为准；画布折线不是引擎连接本身，WireNetworkSolver 会从几何推导 virtual wires；事件引擎与迭代引擎的调度语义不同；缓存用于避免重复构建，不是结果正确性的独立来源。
