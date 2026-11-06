# LogicSim 项目文件结构说明

> 本文档按目录顺序详细说明 LogicSim（Qt6 + QML 数字电路仿真器）中每个文件和文件夹的职责。
> 项目已完成模块化拆分：C++ 源码按依赖层次分到 `src/` 下的 6 个静态库，QML 在根目录 `qml/`，测试在 `tests/`，Android 打包资源在 `android/`。

---

## 目录

- [零、总览](#零总览)
- [一、项目根目录](#一项目根目录)
- [二、`src/core/` —— 基础数据结构与工具（库 `logicsim_core`）](#二srccore--基础数据结构与工具库-logicsim_core)
- [三、`src/logic/` —— 元件规则与连线网络（库 `logicsim_logic`）](#三srclogic--元件规则与连线网络库-logicsim_logic)
- [四、`src/io/` —— 序列化与持久化（库 `logicsim_io`）](#四srcio--序列化与持久化库-logicsim_io)
- [五、`src/move/` —— 移动重连线 A* 子系统（库 `logicsim_move`）](#五srcmove--移动重连线-a-子系统库-logicsim_move)
- [六、`src/engine/` —— 两套仿真引擎（库 `logicsim_engine`）](#六srcengine--两套仿真引擎库-logicsim_engine)
- [七、`src/model/` —— Circuit 主类（库 `logicsim_model`）](#七srcmodel--circuit-主类库-logicsim_model)
- [八、`src/app/` —— 程序入口](#八srcapp--程序入口)
- [九、`qml/` —— QML 前端](#九qml--qml-前端)
- [十、`tests/` —— 模块测试](#十tests--模块测试)
- [十一、`android/` —— Android 打包资源](#十一android--android-打包资源)
- [十二、依赖关系图](#十二依赖关系图)
- [十三、构建与测试](#十三构建与测试)
- [十四、阅读路线建议](#十四阅读路线建议)

---

## 零、总览

### 目录树

```
logicsim/
├── CMakeLists.txt              # 顶层：project、find_package、6 个 add_subdirectory、
│                               #       qt_add_executable(LogicSim)、qt_add_qml_module、
│                               #       enable_testing、add_subdirectory(tests)
├── README.md                   # 项目介绍、构建方式、已知限制
├── LICENSE                     # GPLv3
├── map.md                      # 本文件
├── qtWin.sh                    # Windows 交叉编译（MXE）脚本，未广泛验证
├── 合并.sh                     # 作者自用脚本
├── android/                    # Android 打包资源
│   ├── AndroidManifest.xml
│   └── res/mipmap-*/ic_launcher.png
├── qml/                        # QML 前端（全部在根下）
│   ├── Main.qml
│   ├── Theme.qml
│   └── views/
│       ├── bars/
│       ├── canvas/
│       ├── panels/
│       └── dialogs/
├── src/                        # C++ 源码
│   ├── core/                   # 库 logicsim_core
│   ├── logic/                  # 库 logicsim_logic
│   ├── io/                     # 库 logicsim_io
│   ├── move/                   # 库 logicsim_move
│   ├── engine/                 # 库 logicsim_engine
│   ├── model/                  # 库 logicsim_model
│   └── app/                    # main.cpp（target 在顶层创建）
└── tests/                      # 模块测试（每个模块对应一个子目录）
    ├── CMakeLists.txt
    ├── helpers/
    ├── core/
    ├── logic/
    ├── io/
    ├── move/
    ├── engine/
    └── model/
```

### 依赖层次

```
Qt6::Core
   ↑
logicsim_core          src/core
   ↑
logicsim_logic         src/logic
   ↑
   ├── logicsim_io      src/io
   ├── logicsim_move    src/move
   └── logicsim_engine  src/engine
          ↑
   logicsim_model       src/model
          ↑
   LogicSim（顶层 CMakeLists.txt + qml/）
          ↑
   tests/*（各自链接所需库）
```

**无环**：`core → logic → {io, move, engine} → model → LogicSim`。

- `Qt6::Quick` / `Qt6::QuickControls2` / `Qt6::Qml` 只被可执行文件 `LogicSim` 链接。
- `Qt6::Test` 只被 `tests/` 下的测试链接。
- 各库只用 `Qt6::Core`（`QObject` / `QTimer` / `QSettings` / `QFile` 等）。

### 拆分带来的收益

| 维度 | 拆分前 | 拆分后 |
|---|---|---|
| 改 `ValueCodec.cpp` | 全部 30+ 个 `.cpp` 重编 | 只重编 `logicsim_core` 的 `.cpp`，下游重链接 |
| 测 `ValueCodec` | 必须链接整个 `Circuit`（含引擎、A*） | 只链接 `logicsim_core`，不碰引擎和 A* |
| 加新模块 | 塞进唯一的 `CIRCUIT_SOURCES` | 新 `add_subdirectory` + 独立库 + 独立测试 |
| 架构约束 | 靠人自觉 | `core` 若反向 include `engine/...` 会直接链接失败 |
| 测试需要 GUI | 是（链 `Qt6::Quick`） | 否（只链 `Qt6::Core` + `Qt6::Test`） |

---

## 一、项目根目录

### `CMakeLists.txt`

顶层构建脚本。主要内容：

- `project(LogicSim VERSION 1.0 LANGUAGES CXX)`、C++17、`AUTOMOC ON`。
- `find_package(Qt6 6.5 REQUIRED COMPONENTS Core Qml Quick QuickControls2 Test)`。
- `qt_standard_project_setup(REQUIRES 6.5)`，可选 `qt_policy(SET QTP0004 NEW)` 消除子目录 `.qml` 的 `qmldir` 警告。
- 定义 `LOGICSIM_INCLUDE_ROOT = ${CMAKE_CURRENT_SOURCE_DIR}/src`，作为所有库的 `PUBLIC` include 根。
- `add_subdirectory(src/core)`、`src/logic`、`src/io`、`src/move`、`src/engine`、`src/model`。
- **`qt_add_executable(LogicSim src/app/main.cpp)` 在顶层创建**（不在 `src/app/`），这样 `LogicSim` target 的源目录是项目根，Qt Creator 才能把 `qml/` 显示在根下而不是 `src/app/other locations`。
- `target_link_libraries(LogicSim PRIVATE logicsim_model Qt6::Quick Qt6::QuickControls2 Qt6::Qml)`。
- `if(ANDROID) set_property(TARGET LogicSim PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR ...)`。
- QML 模块：
  - `set(QML_FILES ...)` 与 `set(QML_RESOURCES ...)` 用**相对顶层的路径**（`qml/Main.qml`、`qml/views/canvas/Geometry.js` 等）。
  - 循环给每个文件设 `QT_RESOURCE_ALIAS` 为去掉 `qml/` 前缀的路径（`Main.qml`、`views/canvas/Geometry.js`），这样 `loadFromModule("LogicSim", "Main")` 能按 `qrc:/qt/qml/LogicSim/Main.qml` 找到。
  - **`set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)`**，且必须在 `qt_add_qml_module` **之前**，否则 `Theme.xxx` 在 QML 里全是 `undefined`。
  - `qt_add_qml_module(LogicSim URI LogicSim VERSION 1.0 OUTPUT_DIRECTORY LogicSimQml QML_FILES ... RESOURCES ...)`。
- `enable_testing()` + `add_subdirectory(tests)`。

### `README.md`

项目介绍：为什么做、两套引擎的差异、核心特性、界面说明、支持的元件、快捷键、构建方式、项目结构（与本文件呼应）、已知限制、许可证、致谢。

### `LICENSE`

GPLv3。

### `qtWin.sh`

MXE 交叉编译脚本。作者在 README 里明确说"此脚本在作者环境之外基本无法直接运行，需要自行配置 MXE 工具链"。

### `合并.sh`

作者自用脚本（合并代码片段），与构建无关。

### `map.md`（本文件）

项目文件职责表。**拆分后所有路径已更新为 `src/...`**，包含 `tests/` 说明。

---

## 二、`src/core/` —— 基础数据结构与工具（库 `logicsim_core`）

**职责**：纯数据与工具，无项目内依赖，只依赖 `Qt6::Core`。是整个项目"值语义"基础——撤销、导入、粘贴都靠 `CircuitContext` 可自由拷贝。

### `CircuitContext.h` / `CircuitContext.cpp`

`CircuitContext` 结构体，是**整个工程的"上下文"单位**（根电路或一个子电路）：

```cpp
struct CircuitContext {
    QVariantList components;   // 元件列表，每项是 QVariantMap
    QVariantList wires;        // 连线列表，每项是 QVariantMap
    int compCounter = 0;       // 元件 ID 计数器（C1/C2/...）
    int wireCounter = 0;       // 连线 ID 计数器（W1/W2/...）

    int indexOfComponent(const QString& id) const;
    int indexOfWire(const QString& id) const;
    bool isEmpty() const;
};
```

- **关键认知**：根电路也是 `CircuitContext`，用空串 ID 表示（`m_currentCtxId.isEmpty()`）。所有"取当前上下文"的操作都先看是否为空串。
- 值语义：`components`/`wires` 是 `QVariantList`（隐式共享），拷贝只增引用计数，真正修改时才分裂。
- `indexOfComponent`/`indexOfWire` 线性查找（O(n)），因为规模通常不大，维护哈希索引反而增加撤销/粘贴的复杂度。

### `Component.h`

元件 `QVariantMap` 的 inline 存取器命名空间。集中约定"元件有哪些字段"，避免各处硬编码字符串：

- **字段读取**：`id`、`type`、`name`、`subId`、`displayBase`、`bitWidth`（钳位 `[1,64]`）、`inputCount`、`rotation`、`x`、`y`
- **类型判定**：`isInput`（input/const）、`isEditable`、`isClock`、`isOutput`、`isLed`、`isSub`、`isText`、`isSplitter`、`isHub`、`isGate`、`isTransistor`
- **限定 ID**：`qualifiedId(prefix, localId)` —— prefix 为空直接返回 localId，否则 `prefix + "/" + localId`

全部 inline，不需要 `Component.cpp`。

### `Wire.h`

连线 `QVariantMap` 的 inline 存取器：

- **字段读取**：`id`、`fromComp`、`toComp`、`fromPort`、`toPort`
- **判定**：`connects(w, compId)` —— 源端或目标端任一匹配

与 `Component.h` 对称设计。ortho 线的 T 型引出端可能没有 `fromComp`/`toComp`，调用方需容忍空值。

### `IdAllocator.h` / `IdAllocator.cpp`

ID 发号器命名空间。纯自增计数器 + 格式化：

- `nextComponentId(ctx)` → `"C" + ++ctx.compCounter`
- `nextWireId(ctx)` → `"W" + ++ctx.wireCounter`

每个上下文独立计数，保证子电路内 ID 只在本上下文唯一；跨上下文由实例路径（`"U2/G3"`）区分。

### `UndoData.h`

撤销系统数据结构：

- **`UndoScope`**（`enum class`）：三档范围
  - `CurrentCtx`：只存当前上下文（最常用，省内存）
  - `Full`：存根 + 所有子电路 + 名称表 + 计数器 + 时钟状态 + 导入记录
  - `MetaOnly`：只存名称表 + 子电路计数器
- **`UndoEntry`**：单条快照。按 `scope` 填充对应字段：
  - `ctxId` / `singleCtx`（CurrentCtx 用）
  - `allContexts` / `subNames` / `subCounter` / `clockStates` / `importedFiles`（Full 用）
  - `subNames` / `subCounter`（MetaOnly 用）
  - `mergeKey` / `time`（合并判断用）
- **`CircuitUndoData`**：撤销/重做完整状态容器
  - `std::vector<UndoEntry> undoStack` / `redoStack`
  - `QString lastMergeKey` / `QDateTime lastUndoTime`
  - `bool restoring` / `redoRunning`（两个互斥标志，屏蔽恢复期间入栈）
  - `int maxUndo = 200`（栈深度上限）

**为什么 `CircuitUndoData` 不放在 `Circuit` 成员里**：`Circuit.h` 里只声明 `CircuitUndoData* und() const`，用 `QObject::setProperty` 挂一个 `void*`，避免头文件暴露 `std::vector<UndoEntry>` 等实现细节给所有包含者。

### `ValueCodec.h` / `ValueCodec.cpp`

二进制串与多进制互转。是"值表示层"的唯一实现：

- **`bitsToString(bits, bw)`**：`QVariantList`（元素 bool）→ 定长二进制串；长度 < bw 补 0，> bw 截断。
- **`bitAt(s, idx)`**：安全读某位，越界返回 false。
- **`format(binStr, base, bw)`**：二进制串按 `base` 格式化
  - `"bin"`：补齐/截断
  - `"hex"`：每 4 位一组
  - `"udec"` / `"sdec"`：无符号/有符号十进制，`sdec` 需 `bw` 判断符号位
  - `"f32"` / `"f64"`：`std::memcpy` 位模式 → float/double，`'g', 9` / `'g', 17` 精度输出
- **`parse(text, base, bw, err)`**：文本按 `base` 解析 → `QVariantList`（元素 bool）
  - 二进制只接受 0/1
  - hex 只接受 0-9/A-F
  - udec 不允许负
  - sdec 检查 `[-2^(bw-1), 2^(bw-1)-1]`
  - f32/f64 要求位宽 ≥ 32/64
  - 失败写入 `err` 并返回全 0

**测试**：`tests/core/test_value_codec.cpp` —— format 各 base、parse 非法字符、roundtrip。

### `CMakeLists.txt`

```cmake
add_library(logicsim_core STATIC
    CircuitContext.h  CircuitContext.cpp
    Component.h  Wire.h  UndoData.h
    IdAllocator.h  IdAllocator.cpp
    ValueCodec.h  ValueCodec.cpp
)
target_include_directories(logicsim_core PUBLIC ${LOGICSIM_INCLUDE_ROOT})
target_link_libraries(logicsim_core PUBLIC Qt6::Core)
```

`PUBLIC` include 根让下游直接 `#include "core/ValueCodec.h"`。`PUBLIC` 链接 `Qt6::Core` 让下游自动获得 Qt Core 依赖。

---

## 三、`src/logic/` —— 元件规则与连线网络（库 `logicsim_logic`）

**职责**：元件端口/位宽/组合逻辑的**唯一事实来源** + 几何连线到电气网络的转换。两套引擎（`logicsim_engine`）和移动重连线（`logicsim_move`）都依赖它。

### `ComponentLogic.h` / `ComponentLogic.cpp`

**全项目最重要的一个文件**。作者在注释里明说"这是 logic 层唯一的元件知识入口"。

- **端口数量**：
  - `inputCount(comp)`：数据源/文本 0；`not` 1；`and/or/nand/nor/xor/xnor` 取 `inputCount` 字段（钳位 2..64）；`led/output` 1；`splitter` 1；`hub` 取分段数；`sub` 取 `subInputNames` 长度；`tgate/ntran/ptran` 2。
  - `outputCount(comp)`：数据源 1；`text` 0；门类 1；`splitter` 取分段数；`hub` 1；`led/output` 0；`sub` 取 `subOutputNames` 长度；晶体管 1。
- **端口位宽**：
  - `outputBitWidth(comp, portIdx)`：大多等于 `bitWidth`；`splitter` 取 `outputSplits[portIdx]`；`sub` 取 `subOutputWidths[portIdx]`；越界回退 1。
  - `inputBitWidth(comp, portIdx)`：大多等于 `bitWidth`；`hub` 取 `outputSplits[portIdx]`；`sub` 取 `subInputWidths[portIdx]`；越界回退 1。
- **端口几何**：
  - `outputPorts(comp)`：返回 `QVariantList`，每项 `{index, x, y, bitWidth}`，坐标是**相对元件中心的偏移**。不同类型布局不同（门在右侧、splitter 在右侧多行、晶体管按类型偏移、时钟在右侧中间），最后统一按 `rotation` 旋转（`rotatePoint`：`(x,y)→(-y,x)` 等）。
  - `inputPorts(comp)`：规则与 `outputPorts` 对称。
- **组合逻辑**（两套引擎共用的纯函数）：
  - `computeCombinationalOutputs(type, inputs, bitWidth, outputSplits)` → `QVector<QString>`
  - `not`：逐位取反，Z/E 映射为 E
  - `and/or/nand/nor/xor/xnor`：逐位规约，任一输入含 Z/E 则输出 E，nand/nor/xnor 最后取反
  - `tgate`：EN=1 输出 D、EN=0 输出 Z、EN=Z/E 输出 E
  - `ntran`：G=1 导通、G=0 输出 Z
  - `ptran`：G=0 导通、G=1 输出 Z
  - `splitter`：主输入按 `outputSplits` 从高位到低位切分
  - `hub`：多输入按 `outputSplits` 拼接到主输出（splitter 逆操作）
  - 未识别类型返回空列表

**为什么集中在一处**：两套引擎对同一电路必须输出一致，否则切换引擎后行为不同。`ComponentLogic` 是唯一实现，`EventEngine::computeOutputs` 和 `IterEngine::evalOne` 都调它。

**测试**：`tests/logic/test_component_logic.cpp` —— `inputCount` 数据驱动、`computeAnd` 真值、含 E 输入、splitter 分段。

### `WireNetworkSolver.h` / `WireNetworkSolver.cpp`

连线网络求解：

- **`generateVirtualWires(components, wires)`** → `QVariantList`
  - 用并查集把"元件端口位置"和"wire 路径顶点"合并成电气网络
  - 合并规则：
    1. 元件端口 ↔ 位置节点（`unite`）
    2. wire 内部所有顶点合并（首顶点 + 其余拐点）
    3. 端点重合 → 连通
    4. **T 型接入**：端点落在另一根线中间段 → 连通（等价于 Logisim 分割线后端点对端点）
    5. **十字交叉**（中间对中间）→ **不连通**
  - 最后为每个网络中的每对（输出端口, 输入端口）生成一条 `wireType="virtual"` 的线
  - 虚拟线**只供引擎建图**，不写入上下文、不参与渲染
  - 两套引擎（`IterEngine::evaluateContext`、`Circuit::rebuildEventEngine`）都先调它再合并真实线
- **`connectionDots(components, wires)`** → `QVariantList`
  - 统计每个坐标的"连接数"：元件端口 +1、wire 顶点 +1、T 型接入点额外 +2（模拟该 wire 被分割成两段）
  - 连接数 ≥ 3 的位置返回 `{x, y}`
  - 用计数模拟而非真正分割 wire，是为了在只读展示时保持数据结构不变

**测试**：`tests/logic/test_wire_network_solver.cpp` —— 端点对端点、T 型接入、十字交叉不连通、多驱。

### `SimEngine.h`

```cpp
enum class EngineKind { Event, Iter };
inline EngineKind engineKindFromString(const QString& s);
inline QString engineKindToString(EngineKind k);
```

引擎种类枚举 + 字符串互转。`engineKindFromString` 只接受 `"iter"`，其余回退 `Event`（"默认安全"策略）。用枚举而非多态接口，因为只有两种实现，虚函数与工厂模式的抽象成本不划算。

### `CMakeLists.txt`

```cmake
add_library(logicsim_logic STATIC
    ComponentLogic.h  ComponentLogic.cpp
    WireNetworkSolver.h  WireNetworkSolver.cpp
    SimEngine.h
)
target_link_libraries(logicsim_logic PUBLIC logicsim_core)
```

---

## 四、`src/io/` —— 序列化与持久化（库 `logicsim_io`）

**职责**：存档、剪贴板、最近文件。只依赖 `logicsim_core`。

### `JsonCodec.h` / `JsonCodec.cpp`

存档 JSON 编解码。**字段格式的唯一实现**，`Circuit::toJson`/`fromJson` 只负责收集/填回成员，字段映射全在这里。

- **`encode(root, subContexts, subNames, subCounter, clockFrequency, clockTickCount, engineType, globalGateDelay, wireDelay, simulationSpeed, simulationSpeedExp, autoSpeed, gateTypeDelays)`** → `QVariantMap`
  - 字段：`version: 11`、`root`（components/wires/compCounter/wireCounter）、`subcircuits`（数组，每项 id/name/components/wires/compCounter/wireCounter）、`subCounter`、`clockFrequency`、`clockTickCount`、`engineType`、`globalGateDelay`、`wireDelay`、`simulationSpeed`、`simulationSpeedExp`、`autoSpeed`、`gateTypeDelays`
  - 显式 `QVariant::fromValue<qulonglong>(...)` 避免精度/类型退化
- **`decode(data)`** → `DecodedData`
  - `DecodedData` 字段与 encode 参数一一对应，默认值即"缺字段兜底"
  - 范围钳位：`clockFrequency` `[1,1000]`、`simulationSpeedExp` `[0,12]`、延迟 ≥1
  - 引擎类型白名单校验（非 `iter`/`event` 回退 `event`）
  - 对每个上下文调 `fixCtx`：补 `displayBase`/`name`/`content`、规范化 `inputBits` 长度、补 `outputSplits`、收集 `clockStates`
  - **时钟状态不存文件**：`decode` 时由 `fixCtx` 统一初始化为 false（时钟在加载时从 0 开始）

**测试**：`tests/io/test_json_codec.cpp` —— 往返一致性、缺字段兜底、范围钳位、`fixCtx` 行为。

### `ClipboardCodec.h` / `ClipboardCodec.cpp`

复制/粘贴编解码。

- **`encode(ctx, ids)`** → `QVariantList`（只含一个元素，是 `{components, wires}` 的 map）
  - 只选线：只复制明确选中的线
  - 选了元件：先复制直接相连的线，再**迭代扩散**（端点重合、正向 T 型、反向 T 型），最多 200 轮
- **`paste(ctx, data, dx, dy)`** → `PasteResult { QStringList newIds; QHash<QString,bool> newClockStates; }`
  - 为每个元件分配新 ID（`IdAllocator`），建立旧→新映射
  - 元件坐标整体平移 `(dx, dy)`
  - 连线按映射重写 `fromComp`/`toComp`；ortho 线的 `path` 顶点同样平移
  - **ortho 线允许一端映射失败**（自然悬空，path 已被平移）；**bezier 线要求两端都成功**
  - 新时钟元件登记初始状态 false，供调用者同步到 `m_clockStates`

**测试**：`tests/io/test_clipboard_codec.cpp` —— 往返、ID 映射、ortho 一端悬空、bezier 两端必成功。

### `RecentStore.h` / `RecentStore.cpp`

最近文件持久化。用 QSettings 存一段 JSON 数组字符串。

- `load()`：读 `"recentFiles"` → 解析 JSON 数组 → 逐项 `toVariantMap`；JSON 非法或不是数组时返回空列表，不崩溃。
- `save(list)`：逐项 `QJsonObject::fromVariantMap` 组装 → 序列化为紧凑 JSON 写入 → `s.sync()` 立即落盘，避免异常退出丢记录。
- 不做截断/去重——`Circuit::addRecentFile` 已保证列表规模与顺序。

**测试**：`tests/io/` 里没有单独测试（`load`/`save` 依赖 QSettings，测试可用 `QSettings::setPath` 隔离；目前未写）。

### `CMakeLists.txt`

```cmake
add_library(logicsim_io STATIC
    JsonCodec.h  JsonCodec.cpp
    ClipboardCodec.h  ClipboardCodec.cpp
    RecentStore.h  RecentStore.cpp
)
target_link_libraries(logicsim_io PUBLIC logicsim_core Qt6::Core)
```

---

## 五、`src/move/` —— 移动重连线 A* 子系统（库 `logicsim_move`）

**职责**：用户拖动元件时，用 A* 在避让网格上重新规划被移动元件与外部网络之间的 ortho 连线。**独立子系统**，只依赖 `logicsim_logic`（用 `ComponentLogic` 查端口几何）。

### `AvoidanceMap.h` / `AvoidanceMap.cpp`

网格占用图。稀疏 `QHash<QString, QString>`，key 是 `"x,y"`（取整到 10px 网格），value 是三种常量之一：

- `ALLOW_NEITHER = "neither"`：完全禁止穿越
- `ALLOW_VERTICAL = "vert"`：只允许垂直方向穿越（格点被水平线占用）
- `ALLOW_HORIZONTAL = "horz"`：只允许水平方向穿越（格点被垂直线占用）

接口：

- `create(elements, dx, dy)` 工厂：批量标记元件包围盒 + ortho 线
- `cloneMap()`：复制生成独立地图（A* 过程中会临时标记/取消标记，必须 clone 避免污染缓存）
- `get(x, y)`：查询某坐标的允许方向，未标记返回空串（完全自由）
- `markAll` / `markComponent` / `markWire` / `unmarkWire` / `unmarkLocation`

**关键设计**：
- 元件按类型估算包围盒（输入/输出/子电路/分线器尺寸不同），盒内写 `ALLOW_NEITHER`
- ortho 线沿每段采样（每 10px 一步），水平段写 `ALLOW_VERTICAL`、垂直段写 `ALLOW_HORIZONTAL`；若格子已被相反方向占用则升级为 `ALLOW_NEITHER`（线交叉处禁止再穿）
- bezier 线不占网格（曲线难以精确离散，且 A* 只对 ortho 生效）

### `SearchNode.h` / `SearchNode.cpp`

A* 搜索节点。表示"在某位置、朝某方向、正在延伸某条连接"的状态。

- **惩罚常量**：
  - `CROSSING_PENALTY = 20`：穿越已有线/元件的额外代价（数值不大，保证无路可走时仍能穿）
  - `TURN_PENALTY = 50`：每转一次弯的额外代价（远大于交叉惩罚，因为直角拐点影响美观）
- **两种构造**：
  - 起点构造：`prev=nullptr`、`dist=0`、`extendsWire = !srcDir.isEmpty()`
  - 扩展构造：由 `next()` 调用，携带父指针
- **`next(moveDir, crossing)`**：
  - 按 `moveDir` 前进一格（10px）
  - 累计距离：延伸原线 +9、普通 +10（略优惠鼓励顺着原线走）
  - `crossing` 为真加 `CROSSING_PENALTY`
  - `moveDir != m_dir` 加 `TURN_PENALTY`
  - 越界（负坐标）返回 `nullptr`
- **`getHeuristic()`**：
  - 在延伸原线且目标在原方向前方：用轴向距离 ×9/10 + 横向偏差（乐观估计）
  - 否则用曼哈顿距离
  - 当前方向与目标方向相反：额外加两倍转弯惩罚（惩罚回头路）
  - 起点尚未定方向：目标不在原地则加一次转弯惩罚
- **`hash()` / `equals()`**：按 `(loc, dir, dest)` 判同（不含 dist/prev，因为 A* 中同一状态只应保留最优到达方式）

### `ConnectionData.h`

一条连接的描述：

- `m_loc`：端口当前坐标 `{x, y}`
- `m_dir`：首选延伸方向（`"east"/"west"/"north"/"south"`，空串表示由位移方向推断）
- `m_wirePath`：沿端口向外经过的非选中 ortho wire 列表
- `m_wirePathStart`：沿 wirePath 走到尽头的坐标

`hash()`：x、y、dir 按 31 组合，作为 `pathLocs`/`initNodes` 的 key。

### `MoveRequest.h`

移动请求值对象：

- `m_gesture`：手势指针
- `m_dx, m_dy`：位移
- `equals`：指针 + 位移相同即视为同一请求
- `hash`：`(int)(reinterpret_cast<quintptr>(m_gesture) * 31) * 31 + m_dx * 31 + m_dy`，作为 `m_cachedResults` 的 key

### `MoveResult.h`

计算结果纯数据载体：

- `additions`：新增线
- `removals`：整体删除的原线
- `replacements`：`(原线, 缩短后的新线)` 对（从原线中间引出新线时用）
- `unsatisfied`：未能成功重连的连接（A* 失败或超时）
- `totalDistance`：所有成功路径的总距离（多轮尝试比较优劣用）

### `MoveGesture.h` / `MoveGesture.cpp`

一次拖动会话的对象：

- 构造：保存 `const CircuitContext* m_circuit`（不拥有）+ `QList<QVariantMap> m_selected`（值拷贝，拖动期间上下文可能被修改）
- `getConnections()`（懒计算）：收集选中元件所有端口世界坐标 → 对落在非选中元件端口上的位置，沿非选中 ortho wire 向外走 → 记录 wire 链、链末端坐标与首选方向
- `getFixedAvoidanceMap()`（懒创建）：只含非选中元件与非选中 ortho 线，作为所有位移请求的基底
- `notifyResult(req, result)`：工作线程回调，结果存入 `m_cachedResults[req.hash()]`
- `findResult(dx, dy)`：查询缓存，命中返回指针，未命中返回 `nullptr`
- `enqueueRequest(dx, dy)`：异步提交（非优先入队），已缓存则返回 false
- `forceRequest(dx, dy)`：同步强制计算（优先入队 + 自旋等待缓存出现），用于拖动结束时的最终位置
- **全局 `QMutex g_gestureMutex`** 保护所有 gesture 的结果缓存（用全局锁而非每对象一把，因为手势数量少、临界区极短、实现简单无死锁风险）

**测试**：`tests/move/test_connector.cpp`（占位，A* 用例重，后续补）。

### `Connector.h` / `Connector.cpp`

重连线计算器。**唯一公有接口 `computeWires(req)`**：

1. 从 gesture 取连接描述
2. 用固定避让图剔除不可能的连接（目标被占用且不在原 wire 路径上的）
3. 为每条连接生成 A* 起点（`processConnection`：端口位置 + 沿 wire 链的候选点）
4. **多次尝试不同连接顺序**：
   - 0 条 → 0 轮；1 条 → 1 轮；2 条 → 2 轮；3 条 → 8 轮；≥4 条 → 10 轮
   - 前两轮：按位移方向排序（正序/反序各一次）
   - 其余轮：随机打乱
5. 每轮在克隆地图上跑 A*（`tryList`），保留 `unsatisfied` 最少、`totalDistance` 最短的方案
6. 整体计算截止时间 10 秒
7. 返回 `MoveResult*`（裸指针由调用方 delete；请求被覆盖或超时时返回空 `MoveResult` 非 `nullptr`）

内部辅助：
- `findShortestPath(nodes, pathLocs, avoid)`：A* 主循环，`MAX_ITERS = 20000`，每 64 次检查 `ConnectorThread::isOverrideRequested()` 及时中止
- `convertToPath(last)`：沿父链回溯，方向变化的节点即拐点，逆序输出
- `processPath(path, conn, avoid, result)`：把 A* 路径转成连线增删改；路径未从连接原点出发时处理原 wire 链的删除/缩短
- `processConnection(...)`：为一条连接生成 A* 起点（端口位置 + wire 链候选点）
- `tryList(...)`：按给定连接顺序执行一次完整布线
- `sortConnects(connects, dx, dy)`：按位置在位移方向上的投影排序

### `ConnectorThread.h` / `ConnectorThread.cpp`

单例工作线程，把耗时 A* 放到独立线程，不卡 UI：

- **静态接口**：
  - `instance()`：延迟初始化单例并 `start()`
  - `enqueueRequest(req, priority)`：优先请求插队首、普通请求追加队尾，唤醒工作线程
  - `isOverrideRequested()`：是否有更高优先级请求在排队，供 A* 中途放弃过期计算
- **`run()`**：
  - 加锁等待 `m_queue` 非空
  - 取队首请求，更新 `m_hasPending = !m_queue.isEmpty()`
  - 解锁后调 `Connector::computeWires(req)`
  - **即使 `result` 为 `nullptr`（请求被覆盖/中止）也必须通知**，否则 `MoveGesture::forceRequest` 会永久等待 → UI 死锁
  - `req.gesture()->notifyResult(req, result)`

### `CMakeLists.txt`

```cmake
add_library(logicsim_move STATIC
    AvoidanceMap.h  AvoidanceMap.cpp
    SearchNode.h  SearchNode.cpp
    ConnectionData.h  MoveRequest.h  MoveResult.h
    MoveGesture.h  MoveGesture.cpp
    Connector.h  Connector.cpp
    ConnectorThread.h  ConnectorThread.cpp
)
target_link_libraries(logicsim_move PUBLIC logicsim_logic Qt6::Core)
```

---

## 六、`src/engine/` —— 两套仿真引擎（库 `logicsim_engine`）

**职责**：两套求值引擎。都依赖 `logicsim_logic`（用 `ComponentLogic` 算组合逻辑）。

### `event/` —— 事件驱动引擎（默认）

事件驱动引擎有时间概念，时间只随事件推进；支持门延迟、导线延迟；振荡电路能跑起来，而且永远跑不完——这才是真实电路的样子。

#### `event/EventEngine.h`

类声明：

- **`Node`**：引擎内的节点（每个元件对应一个，`sub` 实例本身不建节点，其内部元件递归建节点）
  - `qid`：限定 ID（`"实例路径/本地ID"`；根上下文元件即本地 ID）
  - `localId`、`type`、`bitWidth`、`raw`（原始元件 map）
  - `inVals` / `outVals`：`QVector<QString>`，各端口当前值
  - `isInput` / `isClock` / `isOutput` / `isLed` 标记
- **`Event`**：
  - `enum Kind { Recalc, Wire }`：重算某节点 / 把某输出值传播到某输入端口
  - `time`：事件发生时间
  - `seq`：同时间事件序号（保证 FIFO 稳定顺序）
  - `nodeIdx` / `port` / `toIdx` / `toPort` / `value`
  - `operator<`：优先队列默认大顶堆，这里反转比较实现小顶堆（时间早者优先，同时间序号小者优先）
- **`Driver`**：`{fromIdx, fromPort}`，某输入端口的一个上游驱动源
- **`Options`**：`globalGateDelay`、`wireDelay`、`runBudget`（默认 200000）
- **三张表**：
  - `m_inDrivers`：输入端口 → 所有驱动源（key 为 `pkey(节点, 输入端口)`）
  - `m_outDests`：输出端口 → 所有目标（key 为 `pkey(节点, 输出端口)`，value 为 `(目标节点, 目标端口)` 列表）
  - `m_portDriverValues`：输入端口 → `(驱动源 dkey → 该源最近一次输出值)`
- **`m_queue`**：`std::priority_queue<Event>`
- **`m_gateDelays`**：按门类型的延迟覆盖表
- **脏节点追踪**：`m_dirtyQids`（`QVector<QString>`）+ `m_dirtySet`（`QSet<QString>` 去重）+ `markDirty` / `takeDirtyQids` / `clearDirtyQids` / `dirtyCount`
- **`pkey(idx, port)`**：`(quint64(idx) << 16) | (port & 0xFFFF)`
- **`dkey(idx, port)`**：同 pkey 布局，但用 qint64（适配嵌套 QHash 的 value key 类型）

#### `event/EventEngine.cpp`

- 构造/析构（`QVector`/`QHash` 自动析构）
- 延迟 getter/setter：`setGlobalGateDelay` / `setWireDelay`（下限钳到 1，避免零延迟导致同一时刻无限重算）、`setGateTypeDelay` / `removeGateTypeDelay` / `clearGateTypeDelays` / `gateDelayFor`（先查覆盖，无则回退全局）
- **`clear()`**：节点、索引、表、队列、计数器、脏集合全部重置
- **`reset()`**（软重置）：保留输入元件当前值，其余节点值清零并重建初始事件。与 `clear` 区别是输入状态不丢，用于"重置仿真但保留输入设置"

#### `event/EventEngine_build.cpp`

- **`build(rootComps, rootWires, subComps, subWires)`**：先 `clear()`，再从根上下文递归建图，最后 `initValues` + `initDriverValues`
- **`buildRecursive(...)`**：
  - 递归深度保护 `depth > 32` 直接停止
  - 第一遍：为本层每个元件建节点（`text` 跳过，`sub` 也建节点但输出由外部驱动）
  - 第二遍：递归展开本层的 `sub` 实例（sub 元件的端口需映射到子电路内部 `input`/`output` 节点）
  - 第三遍：处理本层连线
    - 起点是 `sub` 实例：重定向到子电路内部对应的 `output` 探针节点
    - 终点是 `sub` 实例：重定向到子电路内部对应的 `input` 元件节点
    - 登记 `m_outDests[pkey(fi, fromPort)].append(qMakePair(ti, toPort))` 和 `m_inDrivers[pkey(ti, toPort)].append({fi, fromPort})`
- **`initValues()`**：数据源从 `inputBits` 生成初值；时钟从 `outputPorts` 读初值；其余置零
- **`initDriverValues()`**：建立"驱动源键 → 当前输出值"记录 → 对每个输入端口做多驱合并 → 迭代 32 轮预传播（使 build 后各节点输出接近稳定，减少首帧事件数量）→ 为所有非数据源/非时钟节点调度一次当前时刻的 Recalc 事件

**关键认知**：`build` 后事件队列里只有初始 Recalc 事件，**必须 `run` 一次让队列跑完再 apply**，否则会把默认 0 写回上下文覆盖正确值（代码里多处 `★ 修复` 注释说的就是这个坑）。

#### `event/EventEngine_run.cpp`

核心运行逻辑：

- **`resolveMultiDriver(vals, bw)`**：多驱按位合并
  - 无驱动 → 全 0
  - 单驱动 → 补齐/截断
  - 多驱动 → 逐位：含 E 或 0/1 冲突 → E；否则有 1 → 1、有 0 → 0、全 Z → Z
- **`mapUnary(src, bw)`**：逐位取反，Z/E → E（目前主要为 `not` 提供支持）
- **`scheduleRecalc(idx, at)`** / **`scheduleWirePropagation(fromIdx, fromPort, toIdx, toPort, value, at)`**：入队
- **`step()`**：处理队列中一个最早事件
- **`drainNow(budget)`**：只处理当前时刻（`m_time`）的事件，不推进时间
- **`run(units, budget)`**：推进仿真时间 `units`，处理所有 ≤ endTime 的事件；返回 true 表示已处理到 endTime 或队列提前清空；预算耗尽返回 false（调用方需再次 run）
- **`processEvent(ev)`**：
  - Recalc：直接 `recalcNode(ev.nodeIdx)`
  - Wire：更新 `m_portDriverValues[tkey][dk] = ev.value` → 重新收集该端口所有驱动值 → `resolveMultiDriver` → 与旧值比较，变化才写回并 `recalcNode(ti)`
- **`recalcNode(idx)`**：
  - 调 `computeOutputs(n, n.inVals)` 算新输出
  - 与旧值逐端口比较，变化才标脏并按门延迟调度下游传播
  - 下游事件时间 = `m_time + gateDelayFor(n.type)`
- **`computeOutputs(n, inputs)`**：
  - 数据源/输出探针/LED：输出就是输入值（补齐/截断）
  - 时钟：原样返回当前值（输出由外部 trigger 直接写入）
  - 其余类型：委托 `ComponentLogic::computeCombinationalOutputs`

#### `event/EventEngine_trigger.cpp`

外部触发接口：

- **`triggerInputBit(compId, bit, value)`**：改某 input 元件的单 bit，调度下游传播（时间 = `m_time + m_opts.wireDelay`）
- **`triggerInputString(compId, binStr)`**：整串设置 input 值，调度下游
- **`triggerClockToggle(compId)`**：翻转 clock 输出并调度
- **`triggerForceOutput(compId, port, value)`**：强制设置某节点某端口输出并调度（用于 Circuit 同步 clock 状态、或绕过逻辑直接注入值）
- **`getOutput(compId, port)`** / **`getOutputs(compId)`** / **`getInputs(compId)`**：查询

### `iter/` —— 迭代引擎（旧）

无状态全 static，输入 `CircuitContext` 输出布尔。适合纯组合电路。

#### `iter/IterEngine.h` / `iter/IterEngine.cpp`

- **`evalOne(c, inputMap, comps)`**：
  - 收集输入：按 `inputMap` 查每个输入端口的驱动源列表，读源元件 `outputPorts`，用 `resolveMultiDriverE` 合并
  - 按类型算输出：数据源取 `inputBits`（含 Z/E 时用 `outputOverride`）、时钟原样保留 `outputPorts`、LED/output 取第一个输入、sub 原样保留 `outputPorts`、纯组合逻辑委托 `ComponentLogic`
  - 只在值变化时写回 `c` 并返回 true
- **`evaluateContext(ctx)`**：
  - 生成虚拟线并合并进临时 wire 列表
  - 构建 `inputMap`（目标端口 → 驱动源列表）
  - 拓扑排序（无环时一次收敛）
  - 有环：对环内节点迭代 200 轮
  - 返回本轮是否有任何元件变化
- **`syncSubPins(ctx, subContexts)`**：同步 sub 实例的端口元数据（`subInputNames`/`subOutputNames`/`subInputWidths`/`subOutputWidths`）与端口值数组（数量不符时补齐/截断）
- **`evalInstance(subInst, subContexts, depth)`**：把实例输入值写入子电路内 `input` 元件，递归求值，收集 `output` 元件值作为实例输出
- **`evalRecursive(ctx, subContexts, depth)`**：最多 8 轮外层迭代，交替执行"上下文内迭代"与"子电路实例求值"，处理跨层反馈
- **`evaluateAll(root, subContexts)`**：先对所有上下文调 `syncSubPins`，再反复 `evalRecursive` 直到收敛或 50 轮
- **`evaluateStandalone(ctx, subContexts)`**：只对单个上下文递归求值，不修改传入的 `subContexts`（浏览子电路时用）

### `CMakeLists.txt`

```cmake
add_library(logicsim_engine STATIC
    event/EventEngine.h  event/EventEngine.cpp
    event/EventEngine_build.cpp  event/EventEngine_run.cpp  event/EventEngine_trigger.cpp
    iter/IterEngine.h  iter/IterEngine.cpp
)
target_link_libraries(logicsim_engine PUBLIC logicsim_logic Qt6::Core)
```

---

## 七、`src/model/` —— Circuit 主类（库 `logicsim_model`）

**职责**：`Circuit` 是应用模型层核心：聚合所有状态、对外暴露 QML 接口、内部协调两套引擎与撤销/序列化/子电路系统。继承 `QObject`，禁用拷贝/移动，由 `Main.qml` 里唯一的 `Circuit { id: circuit }` 实例化。

**依赖**：`core + logic + io + move + engine` + `Qt6::Core`。

### `Circuit.h`

主类声明。内容按功能分区：

- **Q_PROPERTY**：`components`/`wires`/`contextId`/`contextName`/`isRootContext`/`isViewOnly`/`subcircuits`/`editContexts`/`errors`/`recentFiles`/`clockRunning`/`clockFrequency`/`clockTickCount`/`canUndo`/`canRedo`/`engineType`（可写）/`simulationRunning`/`simTime`/`globalGateDelay`（可写）/`wireDelay`（可写）/`simulationSpeed`（可写）/`simulationSpeedExp`（可写）/`autoSpeed`（可写）/`showSimTime`（可写）/`wireMode`（可写）/`currentWireType`（可写）/`effectiveWireType`
- **Q_INVOKABLE**：按对象分组（元件/输入值/连线/分线器/子电路/查询/选择/文档/导入/最近/仿真/门延迟/撤销/设置）
- **signals**：`changed`/`geometryChanged`/`structureChanged`/`contextChanged`/`subcircuitsChanged`/`editContextsChanged`/`errorsChanged`/`recentFilesChanged`/`clockChanged`/`undoRedoChanged`/`engineChanged`/`simulationChanged`/`simulationTick`/`engineSettingsChanged`/`showSimTimeChanged`/`wireModeChanged`
- **私有成员**：
  - 电路本体：`Context m_root`、`QHash<QString, Context> m_subContexts`、`QHash<QString, QString> m_subNames`、`QString m_currentCtxId`（空串=根）、`bool m_viewOnly`、`int m_subCounter`、`int m_importCounter`
  - 导航：`QVector<BrowseFrame> m_browseStack`、`QString m_currentInstancePath`
  - 时钟/仿真：`m_clockTimer`、`m_clockStates`、`m_clockRunning`、`m_clockFrequency`、`m_clockTickCount`、`m_engineType`、`m_simulationRunning`、`m_engineDirty`、`m_eventEngine`、`m_simTimer`、`m_simulationSpeed`、`m_simulationSpeedExp`、`m_autoSpeed`、`m_showSimTime`、`m_needFullApply`
  - 撤销：通过动态属性挂 `CircuitUndoData*`
  - 其它：`m_errors`、`m_recentFiles`、`m_importedFiles`、`m_clipboardCache`、`m_ctxSubInstCache`（`mutable`）、`m_wireMode`、`m_currentWireType`
- **私有方法**：`und()`、`currentCtx()`/`ctxById()`、`indexOfComponent`/`Wire`、`evaluateAll`、`loadRecentFiles`/`saveRecentFiles`/`loadSettings`/`saveSettings`、`hasSubCycle`、`pushUndo*`/`captureUndoEntry`/`captureSameScopeAs`/`applyUndoEntry`、`rebuildEventEngine`/`applyEngineOutputsToContexts`/`onSimTimerTick`/`runEventEngineFully`/`engineQidsFor`、`syncSubStateFromParent`/`refreshCurrentSubContext`/`removeSubContextsByPrefix`、`updateWiresForMovedComponent`、四个静态转发 `componentInputCount`/`componentOutputCount`/`outputPortBitWidth`/`inputPortBitWidth`、`dumpInternalState`

**关键设计**：
- `Circuit.h` 里只 `class EventEngine;` 前置声明，不 `#include`，把 `EventEngine.h` 依赖降到 `.cpp` 层
- `CircuitUndoData` 用 `void*` 挂在动态属性上（`kUndoPropName = "__logicsim_undo_v1"`），避免头文件暴露完整定义
- 用 `m_currentCtxId.isEmpty()` 判断根，全项目统一约定

### `Circuit.cpp`

构造/析构与基础 getter/setter：

- 构造：创建 `CircuitUndoData` 挂属性 → `loadRecentFiles()` → 创建 `m_clockTimer` 和 `m_simTimer`（16ms）→ `m_eventEngine = new EventEngine()` → `loadSettings()`
- 析构：释放 `CircuitUndoData`、释放 `m_eventEngine`
- `loadSettings`：读 QSettings 的 `sim/engineType`（白名单）、`sim/globalGateDelay`/`wireDelay`（下限 1）、`simulationSpeedExp`（钳位 `[0,12]`）、`autoSpeed`、`showSimTime`、`clockFrequency`（钳位 `[1,1000]`）、`wireMode`（白名单）、`currentWireType`（白名单）
- `saveSettings`：写回上述所有
- `setShowSimTime`/`setWireMode`/`setCurrentWireType`/`effectiveWireType`：界面偏好的设置
- `und()`：从动态属性读 `void*` 转回 `CircuitUndoData*`
- `canUndo`/`canRedo`
- `currentCtx()`/`ctxById()`：根/子电路的查找，找不到回退根
- `components()`/`wires()`：返回当前上下文的列表拷贝
- `contextName()`：根固定"主电路"，子电路取名称，浏览模式追加"（浏览）"
- `indexOfComponent`/`indexOfWire`：委托 `currentCtx()`
- `simTime`/`globalGateDelay`/`setGlobalGateDelay`/`wireDelay`/`setWireDelay`/`setSimulationSpeed`/`setSimulationSpeedExp`/`setAutoSpeed`：引擎参数 getter/setter，委托 `m_eventEngine` 或私有成员

### `impl/Circuit_lifecycle.cpp`

**主类骨架，最核心的文件**。内容：

- **列表查询**：`subcircuits()`（遍历 `m_subNames` 统计每个子电路 input/output 数量）、`editContexts()`（根 + 所有子电路，ID 升序排序）
- **引擎切换**：`setEngineType(t)`（白名单校验 → 停仿真/停时钟 → 保存设置 → 按类型 rebuild + run + apply 或 apply + evaluateAll → 发信号）
- **求值**：`evaluateAll()`（syncSubPins → 浏览模式同步父输入 → 按引擎类型分派 → 浏览模式单独求值当前子电路 → 发 `subcircuitsChanged`）、`evaluate()`（Q_INVOKABLE 包装）
- **事件引擎生命周期**：
  - `rebuildEventEngine()`（syncSubPins → 拷贝子电路快照 → 判断 `editingSubAsRoot` → 生成虚拟线 → 合并 → `m_eventEngine->build(...)` → 清脏标记、置全量标记、清缓存）
  - `runEventEngineFully(advance, budget)`（反复 run 到空闲，起始 advance 至少 10000，最多重试 200 轮）
  - `applyEngineOutputsToContexts()`（全量/增量模式，内部 `updateOneComponent` 按类型写字段）
  - `engineQidsFor(ctxId, localCompId)`（把本地 ID 翻译成限定 ID 列表，`m_ctxSubInstCache` 缓存）
- **文档生命周期**：`clear()`、`refreshErrors()`（位宽不匹配检查）、`toJson()`/`fromJson()`、`saveToFile()`/`loadFromFile()`（含 `extractFirstJsonObject` 容错提取、`isOnlyWhitespaceFrom` 判断回写修复）
- **外部导入**：`removeSubContextsByPrefix`、`importFromFile`（前缀化导入，处理重复导入/残留清理）、`removeImportedFile`、`importedFiles`
- **撤销系统**：`captureUndoEntry`/`captureSameScopeAs`/`applyUndoEntry`/`pushUndoInternal`/`pushUndo`/`pushUndoAll`/`pushUndoMeta`/`breakUndoMerge`/`undo`/`redo`
- **最近文件**：`loadRecentFiles`/`saveRecentFiles`/`addRecentFile`/`removeRecentFile`/`clearRecentFiles`

**关键认知**：
- `m_engineDirty` 延迟重建（结构变化只置脏）
- `m_needFullApply` 区分刚重建（全量回写）和仿真推进（只回写脏节点）
- `editingSubAsRoot = !m_currentCtxId.isEmpty() && !m_viewOnly` 决定引擎的顶层是谁
- `rebuild` 后必须先 `runEventEngineFully` 再 `apply`（否则默认 0 覆盖正确值）
- 撤销的三档范围（CurrentCtx/Full/MetaOnly）+ mergeKey + 500ms 窗口合并

### `impl/Circuit_component.cpp`

元件相关操作：

- `addComponent(type, x, y, bitWidth, inputCount, subId)`：只读拦截 → 钳位 → 循环引用检测（sub）→ `pushUndo()` → 分配 ID → 按类型初始化端口与字段 → append → 清缓存 → `evaluateAll()` → 发信号 → 返回 ID
- `removeComponent(id)` / `removeComponents(ids)`：删元件并清理关联线（含 T 型接入的线）
- `moveComponent(id, x, y)` / `moveSelection(ids, dx, dy)`：移动并随动 ortho 线（`updateWiresForMovedComponent` 修正首末点与相邻拐点）
- `moveComponentWithWires` / `moveSelectionWithWires`：语义化接口（当前直接转调上面两个）
- `toggleInput(id)`：翻转 input 元件所有位（仿真中走增量触发）
- `setBitValue(id, bit, value)`：设置某一位（仿真中走 `triggerInputBit`）
- `setComponentProp(id, key, value)`：通用属性设置（`bitWidth`/`inputCount`/`rotation` 做钳位与联动，其余直接赋值）
- `rotateComponent` / `renameComponent` / `setSplitterSplits` / `getSplitterSplitsStr`
- `addText` / `setTextContent`

### `impl/Circuit_wire.cpp`

连线相关：

- `findPortAt(x, y, tol)`：端口命中（容差内找最近端口）
- `allWireSegments()`：枚举当前上下文所有 ortho 线段
- `connectionDots()`：委托 `WireNetworkSolver::connectionDots`
- `addOrthoWire(fromX, fromY, toX, toY, direction)`：吸附端口 → 反向时交换端点 + 镜像方向 → 生成 L 型 path（direction==2 先竖后横，否则先横后竖）→ 分配 ID → append → 求值 → 发信号
- `shortenWireEnd(wireId, newX, newY)`：缩短线的一端到新坐标（网格化 10px，离前端更近则缩前端、否则缩末端；新点与保留端重合则整条删；插入拐点保持正交）
- `addWire(fromComp, fromPort, toComp, toPort)`：bezier 线（只记录两端元件与端口索引）
- `removeWire(id)`
- `wireAtPoint(x, y, tol)`：命中测试（ortho 用线段距离、bezier 用采样点距离）
- `setWirePathPoint(id, index, x, y)`：拖中间拐点（网格化 + 相邻点联动保持正交；首末顶点不可拖）
- `getWirePath(id)` / `getWireType(id)`

### `impl/Circuit_subcircuit.cpp`

子电路相关：

- `addSubcircuit(name)`：分配 S 编号、注册上下文与名称、发信号
- `deleteSubcircuit(subId)`：压全量撤销 → 移除上下文与名称 → 遍历所有上下文移除引用它的 sub 实例 → 若正停在被删上下文则退回根 → `evaluateAll` → 发信号
- `setSubcircuitName` / `getSubcircuitName`
- `hasSubCycle(fromCtx, targetId, visited)`：DFS 检测引用环
- `canAddSubInstance(targetSubId)`：自包含或循环引用则禁止
- `refreshCurrentSubContext()`：浏览模式下先从父实例同步输入，再 `IterEngine::evaluateStandalone`
- `enterSubcircuit(subId)`：算实例路径 → 压 `BrowseFrame` 到栈 → 切上下文 → `syncSubStateFromParent` → 置脏 → 独立求值 → 发信号
- `leaveSubcircuit()`：弹栈恢复；栈空则直接退回根
- `switchEditContext(ctxId)`：直接切编辑上下文（清浏览栈）
- `setSubcircuitAsRoot(subId)`：与根上下文互换
- `syncSubStateFromParent(subId)`：从父实例或引擎同步输入值到子电路内部 `input` 元件

### `impl/Circuit_simulation.cpp`

时钟与仿真：

- `tick()`：事件引擎模式下仿真未跑则拒绝；否则翻转所有 clock 电平并刷新 `outputPorts`，走引擎或迭代求值
- `startClock` / `stopClock` / `setClockFrequency`
- `singleStep`：事件引擎推进一个事件；迭代引擎则 `tick()` 一次
- `toggleClock(id)`：手动翻转某个 clock（仿真中走 `triggerForceOutput`）
- `resetAllInputs`：所有时钟归零 + 计数归零
- `setGateTypeDelay` / `getGateTypeDelay` / `clearGateTypeDelays`
- `startSimulation` / `pauseSimulation` / `resetSimulation` / `simulateStep` / `onSimTimerTick`

### `impl/Circuit_selection.cpp`

- `copySelection(ids)`：委托 `ClipboardCodec::encode`，结果缓存到 `m_clipboardCache`
- `pasteSelection(data, dx, dy)`：压撤销 → 委托 `ClipboardCodec::paste` → 登记新时钟状态 → `evaluateAll` → 发信号

### `impl/Circuit_query.cpp`

- `getComponent(id)` / `getWire(id)`
- `outputPortsInfo(id)` / `inputPortsInfo(id)` / `allPortsInfo()`：委托 `ComponentLogic`
- `getInputBase` / `setInputBase` / `formatValue` / `getInputAsString` / `setInputFromString`：值的显示与解析（委托 `ValueCodec`；仿真中走增量触发）
- `statistics()`：递归统计各类元件数量（`visited` 防循环引用）

### `impl/Circuit_catalog.cpp`

四个静态转发函数，委托 `ComponentLogic`：

- `componentInputCount` / `componentOutputCount` / `outputPortBitWidth` / `inputPortBitWidth`

保留这层薄封装是为了让 `.cpp` 内部以 `Circuit` 私有静态接口调用，不把 `ComponentLogic` 暴露给所有调用点。

### `CMakeLists.txt`

```cmake
add_library(logicsim_model STATIC
    Circuit.h  Circuit.cpp
    impl/Circuit_lifecycle.cpp
    impl/Circuit_component.cpp
    impl/Circuit_wire.cpp
    impl/Circuit_subcircuit.cpp
    impl/Circuit_simulation.cpp
    impl/Circuit_selection.cpp
    impl/Circuit_query.cpp
    impl/Circuit_catalog.cpp
)
target_link_libraries(logicsim_model PUBLIC
    logicsim_core
    logicsim_logic
    logicsim_io
    logicsim_move
    logicsim_engine
    Qt6::Core
)
```

**关键**：`Circuit.h` 必须列在源文件里，`AUTOMOC` 才会为 `Q_OBJECT` 生成 moc；漏了会报 `undefined reference to vtable for Circuit`。

**测试**：`tests/model/test_circuit_component.cpp`、`test_circuit_undo.cpp`、`test_circuit_document.cpp`。

---

## 八、`src/app/` —— 程序入口

**只有 `main.cpp`**（没有 `CMakeLists.txt`，因为 `LogicSim` target 在顶层创建）。

### `main.cpp`

- Android 平台设 `QSG_RHI_BACKEND=opengl`（兼容性更好）
- `QGuiApplication app(argc, argv)`
- `app.setOrganizationName("LogicSim")` / `app.setApplicationName("LogicSim")`（供 QSettings 使用）
- `QQuickStyle::setStyle("Basic")`（外观完全由 QML Theme 控制）
- `qmlRegisterType<Circuit>("LogicSim", 1, 0, "Circuit")`（注册 C++ 类型到 QML 模块）
- `QQmlApplicationEngine engine`
- `QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection)`（用**排队连接**保证退出在事件循环运行后生效）
- `engine.loadFromModule("LogicSim", "Main")`
- `return app.exec()`

---

## 九、`qml/` —— QML 前端

**全部在根目录 `qml/`**（由顶层 `qt_add_qml_module` 引用，Qt Creator 项目树显示在根下）。

### 根

#### `Main.qml`

应用主窗口 `ApplicationWindow`。核心内容：

- **`Circuit { id: circuit }`**：模型实例；连接所有信号到缓存刷新/重绘/错误防抖（`onChanged` → `updateCachesValues` + `requestPaint` + `refreshSel` + `errorDebounce.restart`；`onStructureChanged` → `rebuildCachesFull`；`onContextChanged` → 清空选中 + 重建缓存；等等）
- **视图状态**：`viewScale`/`viewX`/`viewY`（缩放与偏移）、`placingType`/`placingSubId`/`placingBitWidth`/`placingInputCount`（放置）、`wiringFromComp`/`wiringFromPort`/`wiring`/`wiringStartIsOutput`/`wireEndX`/`wireEndY`/`wiringStartX`/`wiringStartY`/`wiringDirection`/`shortenWireCandidates`（拉线）、`selectedCompId`/`selectedComp`/`selectedIds`/`selectedSubId`/`selectedWireId`（选中）、`boxSelecting`/`boxStartX`/`boxStartY`/`boxCurrentX`/`boxCurrentY`（框选）、`draggingSelection`/`lastDragWorldX`/`lastDragWorldY`（拖动）
- **其它状态**：`statusText`/`inputError`/`mode`/`errorList`/`clipboardData`/`dirty`/`pendingAfterSave`/`currentFilePath`/`subList`/`editCtxList`/`recentList`/`cachedComps`/`cachedWires`/`compById`/`outPortsById`/`inPortsById`/`lastCompIds`/`lastCompProps`/`visibleComponentCount`/`topCollapsed`/`leftCollapsed`/`settingsOpen`/`forceQuit`/`saveBeforeExit`
- **快捷键**（`Shortcut`）：撤销/重做/删除/复制/粘贴/Back（Android）
- **缓存函数**：`updateCachesValues`（增量）、`rebuildCachesFull`（全量）、`refreshSel`
- **视图函数**：`resetView`/`zoomIn`/`zoomOut`/`triggerRepaint`（16ms 节流）
- **文件函数**：`fileUrlToLocal`/`doSave`/`doSaveAs`/`doOpenFile`/`doExit`/`quitApp`
- **确认函数**：`confirmDelete`、`openRenameSubDialog`
- **布局**：顶部区（`TopBar` + `ClockBar` 或收起条）、只读提示条、左侧面板区（`LeftPanel` 或收起条）、画布区（`CanvasView` + `StatusBar`）、`SettingsPage`、各弹窗实例、两个 `FileDialog`

#### `Theme.qml`

`pragma Singleton` 主题单例。集中定义：

- **颜色**：`bg`/`bgBar`/`bgPanel`/`bgDialogHeader`/`bgDialog`/`bgInput`/`bgButton*`/`bgReadonly`/`bgBack*`；`border`/`borderStrong`/`borderWeak`/`borderDialog`/`borderBtnActive`/`borderDanger*`/`borderSuccess`/`borderSub`/`borderReadonly`/`borderGray`；`text`/`textStrong`/`textWeak`/`textMuted`/`textWhite`/`textGray`/`textFaint`；`accent`/`accentPurple`/`accentOrange`/`accentGreen`/`accentRed`/`accentYellow*`/`accentDirty`/`wireOff`/`grid`
- **尺寸**：`topBarH=42`/`clockBarH=36`/`readonlyBannerH=30`/`leftPanelW=200`/`statusBarH=22`/`buttonRadius=5`/`dialogRadius=10`/`smallRadius=4`/`collapsedBarH=32`/`collapsedBarW=34`
- **字号**：`fsTiny=10`/`fsSmall=11`/`fsNormal=12`/`fsMedium=13`/`fsLarge=14`

**必须在顶层 `CMakeLists.txt` 里设 `QT_QML_SINGLETON_TYPE TRUE`**，否则 `Theme.xxx` 全是 `undefined`。

### `qml/views/bars/`

| 文件 | 内容 |
|---|---|
| `TopBar.qml` | 标题、菜单按钮、模式切换（编辑/控制/选择）、线型切换（可切换模式）、撤销/重做、缩放（缩小/百分比/放大）、上下文按钮、脏标记、收起；暴露 `ctxBtn`/`menuBtn` 供弹窗锚点 |
| `ClockBar.qml` | 仿真开始/暂停/单步/复位（仅事件引擎）、仿真时间、时钟启停/时钟拍/复位输入、频率 SpinBox、拍数 |
| `StatusBar.qml` | `[模式] 状态文本`，有错误时红字 |

### `qml/views/canvas/`

| 文件 | 内容 |
|---|---|
| `CanvasView.qml` | `Canvas` + `paintCanvas`（网格 → 连线 → 圆点 → 拉线预览 → 元件）、`compSize`（带签名缓存）、坐标换算（`worldToScreen`/`screenToWorld`/`portPos`/`bitIndexAt`）、`computeWireSources`（递归追上游给连线着色）、`computeNearestAngle`/`jumpToNearest`（导航指示器）、框选框、导航指示器、`MouseArea` 转发事件到交互 JS；暴露 `requestPaint`/`canvasWidth`/`canvasHeight` |
| `Geometry.js` | `worldToScreen`/`screenToWorld`/`isVisible`（包围盒可见性）/`roundRect`（圆角矩形路径） |
| `Painter.js` | `paintGrid`、`pickWireStyle`/`pickWireColor`（按信号 0/1/Z/E 选颜色虚线）、`drawWire` 分派、`drawWireBezier`/`drawWireOrtho`（按 wireSources 着色、选中高亮）、`drawConnectionDots`、`drawPendingBezier`/`drawPendingOrtho`（拉线预览）、`classifyPortStr`/`fillPortColor`（端口状态色）、`drawComponent`（按类型画形状/文字/端口/名称） |
| `interactions/CanvasInteraction.js` | `findPortAt`/`resolveEffectiveWireType`/`snapWorldPoint`（吸附端口或 10px 网格）/`snapToWire`（吸附到 ortho 线最近点）/`updateWiringDirection`（拐角方向切换）/`pointOnSegInterior`/`findShortenTargetAt`/`startWiringFromPort`/`startWiringFromWire`、`onPressed`（放置/命中端口/命中线/T 型引出/命中元件/空白平移）、`onPositionChanged`（框选/拖动选中/拉线预览/拖单件/平移）、`onReleased`（框选收集/拉线结束：缩短或建线）、`onCanceled`、`onWheel`（以鼠标为锚点缩放）、`onDoubleClicked`（双击子电路实例进入浏览） |
| `painters/` | **空目录**（可删，或未来拆 `Painter.js` 用） |

### `qml/views/panels/`

| 文件 | 内容 |
|---|---|
| `LeftPanel.qml` | 元件库（按分组：输入输出/基本逻辑门/复合逻辑门/传输门·晶体管/工具/控制；子电路列表支持右键/长按菜单：选中/重命名/设为主电路/删除；新建子电路按钮）+ 属性面板 + 重命名子电路对话框；暴露各输入框 alias |
| `PropertyPanel.qml` | 按选中对象显示：多选操作（复制/粘贴/删除）、子电路属性（`SubcircuitPanel`）、连线属性（ID/类型/删除）、元件属性（名称/方向/位宽/输入数/门延迟覆盖/显示进制 ComboBox/LED 颜色/非二进制值编辑/二进制逐位开关/分线器分段/文本内容/晶体管端口说明/删除按钮）、错误列表 |
| `SubcircuitPanel.qml` | 子电路属性子面板：名称编辑、ID/端口数信息、重命名/设为主电路/进入浏览/删除按钮 |
| `props/LedProps.qml` | LED 颜色色块行 |
| `props/SplitterProps.qml` | 分线器/集线器分段文本编辑框（逗号分隔，和=位宽） |
| `props/TextProps.qml` | 文本元件内容编辑框 |

### `qml/views/dialogs/`

| 文件 | 内容 |
|---|---|
| `ConfirmDialog.qml` | 通用确认框：标题 + 消息 + 取消/确定（确定执行 `action` 回调） |
| `CloseConfirmDialog.qml` | 关闭（退出）时有未保存修改：取消/不保存/保存（保存后退出，失败转另存为） |
| `ExitConfirmDialog.qml` | 无未保存修改时的退出确认：取消/退出 |
| `UnsavedDialog.qml` | 执行打开/新建前有未保存修改：取消/不保存（执行 `pendingAction`）/保存（有路径直接存，无路径转另存为并把回调交给 `pendingAfterSave`） |
| `SubNameDialog.qml` | 新建子电路：输入名称，回车或点创建调 `addSubcircuit` |
| `ContextPopup.qml` | 上下文选择弹窗：列出根 + 所有子电路，点击 `switchEditContext` |
| `MenuPopup.qml` | 主菜单：保存/另存为/打开/导入…/已导入…/最近/统计/新建/设置；内部组件 `MenuRow` |
| `RecentPopup.qml` | 最近打开文件：列出条目（名称/时间/路径）、点击打开（含委托重建后引用失效的闭包捕获修复）、单条删除、清空 |
| `StatsPopup.qml` | 电路统计：用 `ListModel` 显示各类元件数量（递归含子电路） |
| `ImportedFilesPopup.qml` | 已导入文件：列出（基名/电路数/路径）、取消导入按钮 |
| `SettingsPage.qml` | 全屏设置页：仿真引擎选择、连线模式选择、传播延迟（门/导线，仅事件引擎）、仿真速度（自动/指数/显示仿真时间）、应用并返回/恢复默认；`syncFromCircuit`/`apply`/`resetToDefaults` 函数 |

---

## 十、`tests/` —— 模块测试

**职责**：按模块对 C++ 层做单元测试。**不测试 QML**（需要 Qt Quick Test + 模拟鼠标事件，投入产出比低；先用 C++ 测试把模型层锁住）。

**框架**：`Qt Test`（`Qt6::Test`）。纯函数测试用 `QTEST_GUILESS_MAIN`，`Circuit` 测试也用（`Circuit` 只用 `QCoreApplication`，不需要 GUI）。

### `tests/CMakeLists.txt`

定义 `logicsim_add_test(path LIBS ...)` 函数：

- **target 名不能含 `/`**（CMake 限制），所以 `core/test_value_codec` 转成 target 名 `core_test_value_codec`（`string(REPLACE "/" "_" target_name ...)`）
- `qt_add_executable(${target_name} ${path}.cpp)`
- `target_include_directories(... PRIVATE ${TEST_INCLUDE})`（指向 `tests/`，让测试能 `#include "helpers/TestHelpers.h"`）
- `target_link_libraries(... PRIVATE Qt6::Test ${T_LIBS})`
- `add_test(NAME ${target_name} COMMAND ${target_name})`

已注册的测试（按模块分组，每个只链接所需的最小库集合）：

| 测试 path | 链接的库 |
|---|---|
| `core/test_value_codec` | `logicsim_core` |
| `core/test_circuit_context` | `logicsim_core` |
| `core/test_id_allocator` | `logicsim_core` |
| `logic/test_component_logic` | `logicsim_logic` |
| `logic/test_wire_network_solver` | `logicsim_logic` |
| `io/test_json_codec` | `logicsim_io` |
| `io/test_clipboard_codec` | `logicsim_io` |
| `engine/test_iter_engine` | `logicsim_engine` |
| `engine/test_event_engine` | `logicsim_engine` |
| `move/test_connector` | `logicsim_move` |
| `model/test_circuit_component` | `logicsim_model` |
| `model/test_circuit_undo` | `logicsim_model` |
| `model/test_circuit_document` | `logicsim_model` |

### `tests/helpers/`

| 文件 | 内容 |
|---|---|
| `TestHelpers.h` | `makeComp(type, x, y, bw, ic)`（构造元件 map）、`withId(c, id)`、`pt(x, y)`（构造点）、`makeOrthoWire(id, path, fromComp, fromPort, toComp, toPort)`、`addComp(ctx, c)`/`addWire(ctx, w)` |
| `EngineTestHelpers.h` | `runToIdle(EventEngine&, step = 100000, budget = 2000000)`：反复 `run` 直到 `isIdle()`（`build` 后必须跑到空闲，否则拿到初始占位值） |

### `tests/core/`

| 文件 | 状态 | 内容 |
|---|---|---|
| `test_value_codec.cpp` | **已写用例** | `format_bin_pads_and_truncates`、`format_hex`、`format_sdec_negative`、`parse_rejects_invalid_bin`（其余待补：`roundtrip_all_bases`、f32/f64、边界位宽） |
| `test_circuit_context.cpp` | 占位 | 应测：`indexOfComponent`/`indexOfWire` 命中与未命中、`isEmpty` |
| `test_id_allocator.cpp` | 占位 | 应测：`nextComponentId` 连续自增、`nextWireId` 独立计数 |

### `tests/logic/`

| 文件 | 状态 | 内容 |
|---|---|---|
| `test_component_logic.cpp` | **已写用例** | `inputCount`（数据驱动：input/not/and/and_0/and_65，验证钳位）、`computeAnd`、`computeAndWithError`、`splitter_splits_by_outputSplits`（其余待补：`outputPorts` 几何、旋转、tgate/ntran/ptran 三态、hub 与 splitter 互逆） |
| `test_wire_network_solver.cpp` | 占位 | 应测：端点对端点、T 型接入、十字交叉不连通、多驱、`connectionDots` 连接数 ≥ 3 |

### `tests/io/`

| 文件 | 状态 | 内容 |
|---|---|---|
| `test_json_codec.cpp` | 占位 | 应测：encode→decode 往返、缺字段兜底、范围钳位、`fixCtx` 补字段 |
| `test_clipboard_codec.cpp` | 占位 | 应测：encode→paste 往返、ID 映射、ortho 一端悬空、bezier 两端必成功、T 型扩散 |

### `tests/engine/`

| 文件 | 状态 | 内容 |
|---|---|---|
| `test_iter_engine.cpp` | 占位 | 应测：纯组合真值、多驱冲突 → E、环路不崩溃、子电路递归、`syncSubPins` 补齐端口 |
| `test_event_engine.cpp` | 占位 | 应测：基本传播、门延迟阻塞、线延迟、按类型覆盖延迟、多驱合并、脏节点追踪、子电路展开、`reset` 保留输入、预算耗尽 |

### `tests/move/`

| 文件 | 状态 | 内容 |
|---|---|---|
| `test_connector.cpp` | 占位 | 应测：`AvoidanceMap` 标记/取消/克隆、`SearchNode` 启发值、单连接布线、多连接顺序、超时返回 `unsatisfied` |

### `tests/model/`

| 文件 | 状态 | 内容 |
|---|---|---|
| `test_circuit_component.cpp` | 占位 | 应测：`addComponent` 字段初始化、`removeComponent` 清理关联线、`moveComponent` 随动 ortho 线、`setComponentProp` 钳位与联动 |
| `test_circuit_undo.cpp` | 占位 | 应测：`addComponent`→`undo`→空、`redo` 恢复、连续移动合并、`breakUndoMerge`、`deleteSubcircuit`+`undo` 完整恢复 |
| `test_circuit_document.cpp` | 占位 | 应测：`toJson`→`fromJson` 往返、`saveToFile`/`loadFromFile`、含垃圾尾巴的 JSON 治愈、`refreshErrors` 位宽不匹配 |

---

## 十一、`android/` —— Android 打包资源

| 路径 | 内容 |
|---|---|
| `AndroidManifest.xml` | 应用名 LogicSim、`org.qtproject.qt.android.bindings.QtApplication`/`QtActivity`、横屏（`sensorLandscape`）、单顶启动、MAIN/LAUNCHER intent、`lib_name=LogicSim`、`extract_android_style=minimal`、INTERNET 权限 |
| `res/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher.png` | 各密度启动图标（二进制） |

由顶层 `CMakeLists.txt` 的 `if(ANDROID) set_property(TARGET LogicSim PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android")` 引用。

---

## 十二、依赖关系图

```
                         ┌─────────────────────────────────────┐
                         │           Qt6::Core                  │
                         └─────────────────┬───────────────────┘
                                           │
                         ┌─────────────────▼───────────────────┐
                         │        logicsim_core                 │
                         │  CircuitContext / Component / Wire   │
                         │  IdAllocator / UndoData / ValueCodec │
                         └─────────────────┬───────────────────┘
                                           │
                         ┌─────────────────▼───────────────────┐
                         │        logicsim_logic                │
                         │  ComponentLogic / WireNetworkSolver  │
                         │  SimEngine                           │
                         └───┬──────────┬──────────┬───────────┘
                             │          │          │
              ┌──────────────▼──┐  ┌────▼──────┐  ┌▼──────────────────┐
              │  logicsim_io     │  │logicsim_  │  │ logicsim_engine   │
              │  JsonCodec       │  │move       │  │ event/EventEngine │
              │  ClipboardCodec  │  │A* 重连线  │  │ iter/IterEngine   │
              │  RecentStore     │  │           │  │                   │
              └──────────┬───────┘  └────┬──────┘  └─────────┬─────────┘
                         │               │                    │
                         └───────────────┼────────────────────┘
                                         │
                         ┌───────────────▼─────────────────────┐
                         │        logicsim_model                │
                         │  Circuit（QObject）                  │
                         │  impl/Circuit_*.cpp 分部实现         │
                         └───────────────┬─────────────────────┘
                                         │
                         ┌───────────────▼─────────────────────┐
                         │  LogicSim（顶层 CMakeLists + qml/）  │
                         │  链接 Qt6::Quick/QuickControls2/Qml  │
                         └───────────────┬─────────────────────┘
                                         │
                         ┌───────────────▼─────────────────────┐
                         │  tests/*（各自链接所需最小库）       │
                         │  链接 Qt6::Test，不链接 Qt6::Quick   │
                         └─────────────────────────────────────┘
```

**约束**：
- `core` 不能 include `logic`/`io`/`move`/`engine`/`model`（CMake 层面没链接，会直接编译失败）。
- `logic` 不能 include `io`/`move`/`engine`/`model`。
- `io`/`move`/`engine` 彼此不依赖，只依赖 `logic`。
- `model` 依赖全部下层，`LogicSim` 只依赖 `model` + Qt Quick 系列。
- `tests/*` 各自链接所需最小库，不拖入无关依赖。

---

## 十三、构建与测试

### 桌面端构建

```bash
git clone https://github.com/zwx-termuxgood/logicsim.git
cd logicsim
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.5+/gcc_64
cmake --build build -j4
./build/src/app/LogicSim
```

**注意**：Qt Creator 里构建时，`LogicSim` target 的源目录是项目根（顶层 `CMakeLists.txt` 里 `qt_add_executable(LogicSim src/app/main.cpp)`），所以 `qml/` 会显示在项目树根下，而不是 `src/app/other locations`。

### 测试

```bash
ctest --test-dir build --output-on-failure
```

或只跑某个测试：

```bash
./build/tests/core_test_value_codec
./build/tests/logic_test_component_logic
```

### 增量编译验证（拆库是否真的生效）

```bash
touch src/core/ValueCodec.cpp
cmake --build build -- -n 2>&1 | grep -E 'Building|Linking'
# 应只看到 logicsim_core 的 .cpp 被重编 + 下游库的链接行
# 不应看到 engine/、move/、model/ 的 .cpp 被重编
```

### Windows 交叉编译（MXE）

```bash
chmod +x qtWin.sh
./qtWin.sh
```

> ⚠ 此脚本在作者环境之外基本无法直接运行，需要自行配置 MXE 工具链。

### Android

用 Qt Creator 打开 `CMakeLists.txt`，选择 Android Kit 构建部署。`main.cpp` 在 Android 下会设 `QSG_RHI_BACKEND=opengl`；`android/AndroidManifest.xml` 配置了横屏、单顶启动、`lib_name=LogicSim`。

---

## 十四、阅读路线建议

按"从外到内、从数据到引擎再到界面"的顺序：

### 第 0 步：先建立全局印象（15 分钟）

1. 读本文件（`map.md`）
2. 读 `CMakeLists.txt` —— 知道有哪些库、谁依赖谁
3. 读 `README.md` —— 知道项目能干什么

### 第 1 步：吃透数据模型（30 分钟）

1. `src/core/CircuitContext.h` —— 上下文 = components + wires + 两个计数器；根也是 Context（空串 ID）
2. `src/core/Component.h` 和 `src/core/Wire.h` —— 数据字典（字段名靠约定）
3. `src/core/ValueCodec.h` —— 值怎么表示（内部统一二进制串）

**目标**：看到任何 `c.value("xxx")` 都能立刻说出它是什么。

### 第 2 步：元件规则（45 分钟）

1. `src/logic/ComponentLogic.h` —— 只读函数签名和注释
2. `src/logic/ComponentLogic.cpp` —— 挑 `not`/`and`/`splitter` 三个类型读实现，其余略过

**关键认知**：`ComponentLogic` 是元件知识唯一入口，两套引擎都依赖它。

### 第 3 步：电气网络（30 分钟）

`src/logic/WireNetworkSolver.h` + `generateVirtualWires` 的 `.cpp`

**关键认知**：用户在画布上画的是几何线，引擎需要电气连接。并查集把端口/顶点合并成网络，T 型接入连通、十字交叉不连通。

### 第 4 步：两套引擎（选一套精读）

- **先读迭代引擎**（`src/engine/iter/IterEngine`）：无状态、全 static、拓扑排序 + 环路迭代。读完就理解了"求值"的基本语义。
- **再读事件引擎**（`src/engine/event/EventEngine`）：
  1. `EventEngine.h` —— 先看 `Node`/`Event`/三张表/`pkey`/`dkey`，数据结构看懂了算法就好懂
  2. `EventEngine_build.cpp` —— `buildRecursive`/`initValues`/`initDriverValues`
  3. `EventEngine_run.cpp` —— `run`/`processEvent`/`recalcNode`/`computeOutputs`
  4. `EventEngine_trigger.cpp` —— 外部触发接口

**关键认知**：事件引擎不"从头算到尾"，而是"值变了才调度下游事件"；`recalcNode` 里"与旧值比较"是性能关键。

### 第 5 步：Circuit 主类（2 小时）

1. `src/model/Circuit.h` 成员变量区 → signals 区 → Q_INVOKABLE 列表
2. `src/model/impl/Circuit_lifecycle.cpp` —— **主类骨架，最核心**：
   - `evaluateAll()`（最核心的求值入口）
   - `rebuildEventEngine()`（把上下文树拍平成引擎节点图）
   - `applyEngineOutputsToContexts()`（引擎结果回写）
   - `setEngineType()`（切换引擎）
   - `toJson`/`fromJson`/`saveToFile`/`loadFromFile`
   - 撤销系统（`pushUndo*`/`captureUndoEntry`/`applyUndoEntry`/`undo`/`redo`）
3. 按需读分部文件：
   - `Circuit_component.cpp` —— 元件操作（`addComponent`/`moveComponent` 含 `updateWiresForMovedComponent`）
   - `Circuit_wire.cpp` —— 连线操作（`addOrthoWire` 的 L 型 path、`shortenWireEnd`、`wireAtPoint`）
   - `Circuit_subcircuit.cpp` —— 子电路进出栈、`syncSubStateFromParent`
   - `Circuit_simulation.cpp` —— 时钟 tick、仿真启停
   - `Circuit_selection.cpp` / `Circuit_query.cpp` / `Circuit_catalog.cpp` —— 篇幅小，最后扫

### 第 6 步：移动重连线（可选，独立子系统）

`src/move/` 是自包含的 A* 寻路子系统：

- `AvoidanceMap` —— 网格占用图
- `SearchNode` —— A* 节点（转弯惩罚、穿越惩罚、延伸原线优惠）
- `Connector::computeWires` —— 入口（剔除不可达 → 生成起点 → 多轮尝试不同顺序 → 保留最优）
- `ConnectorThread` —— 单例工作线程
- `MoveGesture` —— 一次拖动会话，缓存连接描述、固定避让图、位移结果

**如果你对 A* 或"拖动时自动重连线"感兴趣就精读；否则知道"它存在、跑在独立线程"即可。**

### 第 7 步：QML 前端（2 小时）

1. `qml/Main.qml` —— 按顺序看：`Circuit { id: circuit }` 块 → 缓存函数 → 视图状态属性 → 布局块
2. `qml/views/canvas/CanvasView.qml` —— `paintCanvas` 是绘制入口
3. `qml/views/canvas/interactions/CanvasInteraction.js` —— **交互的核心**（`onPressed`/`onPositionChanged`/`onReleased`）
4. `qml/views/canvas/Painter.js` —— 所有 Canvas 绘制（先看 `pickWireStyle` 和 `drawComponent` 的分类型 switch）
5. `qml/views/canvas/Geometry.js` —— 只有 4 个函数，5 分钟读完
6. `qml/views/panels/` 与 `qml/views/dialogs/` —— 用到再看

### 第 8 步：三个横向专题（可穿插读）

1. **撤销系统**：`src/core/UndoData.h` → `Circuit_lifecycle.cpp` 里的 `pushUndoInternal`/`captureUndoEntry`/`applyUndoEntry`/`undo`/`redo`
2. **子电路**：`Circuit_subcircuit.cpp` + `IterEngine::syncSubPins` + `EventEngine_build` 里 sub 实例的重定向
3. **序列化**：`src/io/JsonCodec` → `Circuit_lifecycle.cpp` 的 `toJson`/`fromJson` → `saveToFile`/`loadFromFile` → `importFromFile`

### 几个"读的时候会卡住"的点

1. **`m_currentCtxId` 空串 = 根**。全项目统一约定。
2. **`editingSubAsRoot = !m_currentCtxId.isEmpty() && !m_viewOnly`**：决定"引擎的顶层是谁"。
3. **引擎的限定 ID**：根元件 `"G1"`；子电路实例内元件 `"U2/G3"`。`engineQidsFor` 负责翻译。
4. **"先捕获快照、再改数据"**：`pushUndo` 必须在真正修改之前调用。
5. **`rebuild` 后必须先 `runEventEngineFully` 再 `apply`**：代码里多处 `★ 修复` 注释说的就是这个坑。
6. **QML 的 `main` 属性**：每个子组件都有 `property QtObject main: null`，由 `Main.qml` 注入 `root`。
7. **`Theme.qml` 是 singleton**：必须在 CMake 里设 `QT_QML_SINGLETON_TYPE TRUE`，否则全是 `undefined`。

---

## 附：拆分前后对照表

| 拆分前路径 | 拆分后路径 | 所属库 |
|---|---|---|
| `circuit/core/` | `src/core/` | `logicsim_core` |
| `circuit/logic/ComponentLogic.*` | `src/logic/ComponentLogic.*` | `logicsim_logic` |
| `circuit/logic/WireNetworkSolver.*` | `src/logic/WireNetworkSolver.*` | `logicsim_logic` |
| `circuit/logic/SimEngine.h` | `src/logic/SimEngine.h` | `logicsim_logic` |
| `circuit/logic/event/` | `src/engine/event/` | `logicsim_engine` |
| `circuit/logic/iter/` | `src/engine/iter/` | `logicsim_engine` |
| `circuit/logic/move/` | `src/move/` | `logicsim_move` |
| `circuit/io/` | `src/io/` | `logicsim_io` |
| `circuit/Circuit.h` / `Circuit.cpp` | `src/model/Circuit.h` / `Circuit.cpp` | `logicsim_model` |
| `circuit/app/` | `src/model/impl/` | `logicsim_model` |
| `main.cpp` | `src/app/main.cpp` | `LogicSim`（顶层 target） |
| `qml/` | `qml/`（不变） | `LogicSim`（顶层 `qt_add_qml_module`） |
| `android/` | `android/`（不变） | `LogicSim`（Android 打包） |
| （无） | `tests/` | 各测试 target |

**`#include` 规则**：拆分后所有项目内 include 以 `src/` 为根（`"core/ValueCodec.h"`、`"logic/ComponentLogic.h"`、`"engine/event/EventEngine.h"`、`"move/Connector.h"`、`"io/JsonCodec.h"`、`"model/Circuit.h"`）。
