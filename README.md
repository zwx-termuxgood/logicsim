# LogicSim

一个基于 Qt 6 / QML 的**数字逻辑电路模拟器**，支持多比特位宽、子电路嵌套、时钟仿真、晶体管级建模（含高阻态）、撤销/重做，可运行于桌面端与 Android。
鉴于logisim无法在移动端运行 我借助ai写了这个项目（连线是点对点模式而不是logisim那样 这样在移动端小屏幕好操作）
不过我只能做到尽量方便操作 可能还是有很多问题

## 功能特性

- **多比特位宽**：元件支持 1 ~ 64 位，可按二进制 / 无符号十进制 / 有符号十进制 / 十六进制 / 单精度浮点 / 双精度浮点显示与输入。
- **完整逻辑门库**：AND / OR / NOT / NAND / NOR / XOR / XNOR。
- **晶体管级建模**：传输门、NMOS、PMOS，支持高阻态 `Z`（连线显示为橙色虚线）。
- **子电路**：支持自定义、嵌套调用、循环引用检测、设为主电路、只读浏览、端口自动同步。
- **分线器 / 集线器**：可配置分段，用于位拼接与位拆分。
- **时钟仿真**：频率 1 ~ 1000 Hz，支持运行 / 停止 / 单步 / 复位。
- **撤销 / 重做**：最多 200 步，连续操作自动合并。
- **组合 + 时序电路**：拓扑排序求解组合逻辑，对环路（锁存器、触发器等）迭代收敛。
- **多模式交互**：编辑 / 控制 / 选择（框选、多选、复制、粘贴、批量删除）。
- **画布缩放平移**：滚轮缩放（0.15x ~ 4.0x），空白处拖动平移。
- **位宽检查**：自动检测连线两端位宽不匹配。
- **JSON 存档**：带版本号，兼容旧文件。
- **跨平台**：桌面端 + Android + Windows 交叉编译。(我的设备运行在debian上 因为win的exe需要交叉编译 交叉编译工具链我自己配置的 那个qtWin.sh脚本可能无法在其他设备上运行)


## 界面说明

### 顶部栏

| 按钮 | 说明 |
|------|------|
| 编辑 / 控制 / 选择 | 切换交互模式 |
| ↶ / ↷ | 撤销 / 重做 |
| 保存 / 另存为 / 打开 | 文件操作 |
| 最近 | 最近打开的文件 |
| 统计 | 电路统计（递归含子电路） |
| 新建 | 清空当前电路 |
| − / 100% / + | 缩放 |
| 上下文选择器 | 切换主电路 / 子电路 |

### 时钟栏

运行 / 停止、单步、复位、频率调节、拍数显示。

### 画布操作

- **放置元件**：左侧选中类型 → 点击画布放置
- **拖动元件**：编辑模式下直接拖动
- **连线**：点击端口圆点，再点击目标端口
- **框选**：选择模式空白处拖动
- **进入子电路**：双击子电路实例
- **缩放 / 平移**：滚轮缩放、空白处拖动

### 快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl + Z` | 撤销 |
| `Ctrl + Shift + Z` / `Ctrl + Y` | 重做 |
| `Back`（Android） | 退出流程 |

## 支持的元件

| 分类 | 元件 |
|------|------|
| 输入输出 | 输入开关、输出端口、LED |
| 基本逻辑门 | 与门、或门、非门 |
| 复合逻辑门 | 与非、或非、异或、同或 |
| 传输门 / 晶体管 | 传输门、NMOS、PMOS |
| 工具 | 分线器、集线器 |
| 控制 | 时钟、文字 |
| 子电路 | 自定义 |

## 构建

### 依赖

- Qt 6（Core / Gui / Qml / Quick / QuickControls2）
- CMake 3.16+
- C++17 编译器

### 桌面端

```bash
git clone https://github.com/zwx-termuxgood/logicsim.git
cd logicsim
mkdir build && cd build
cmake ..
cmake --build . -j4
./LogicSim
```

### Windows 交叉编译（MXE）
#### 此脚本可能无法运行（好吧不是可能 基本无法运行）
```bash
chmod +x qtWin.sh
./qtWin.sh
```

脚本会检查 MXE 环境、选择静态/动态链接、编译并处理 DLL 依赖（动态版本输出到 `deploy/`）。

### Android

用 Qt Creator 打开 `CMakeLists.txt`，选择 Android Kit 构建部署。

`main.cpp` 已针对 Android 强制使用 OpenGL ES 后端。

## 文件格式

保存为 JSON，顶层结构：

```json
{
  "version": 10,
  "root": {
    "components": [],
    "wires": [],
    "compCounter": 0,
    "wireCounter": 0
  },
  "subcircuits": [],
  "subCounter": 0,
  "clockFrequency": 2,
  "clockTickCount": 0
}
```

加载时自动补齐缺失字段，兼容旧文件。

## 项目结构

```
logicsim/
├── circuit/                    # C++ 核心逻辑
│   ├── circuit.h / .cpp        # Circuit 类（对 QML 暴露）
│   ├── CircuitContext.h/.cpp   # 电路上下文
│   ├── CircuitEvaluator.h/.cpp # 求值引擎
│   ├── ComponentTraits.h/.cpp  # 元件基础属性
│   ├── PortGeometry.h/.cpp     # 端口几何
│   ├── ValueCodec.h/.cpp       # 进制编解码
│   ├── UndoData.h              # 撤销数据结构
│   └── circuit_*.cpp           # 按功能拆分
├── qml/                        # QML 界面
│   ├── Main.qml                # 主窗口
│   ├── CanvasView.qml          # 画布
│   ├── CanvasPainter.js        # 绘制逻辑
│   ├── Geometry.js             # 坐标转换
│   ├── Theme.qml               # 主题
│   └── *Dialog.qml / *Popup.qml
├── main.cpp
├── qtWin.sh
└── CMakeLists.txt
```

## 许可证

[GPLv3](LICENSE)

```
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
```

## 声明

- 本项目代码主要由 AI 辅助生成，作者进行了调试 优化等 
- 软件按"原样"提供，不附带任何担保。作者不对因使用本软件造成的任何损失负责，完整条款以 [GPLv3](LICENSE) 正文为准。
- 本软件适合学习、实验与个人使用，请勿用于对可靠性有严格要求的场景。
- 再次强调 这是我借助ai辅助生成的 仅供娱乐！！！需要专业电路仿真可以试试Logisim

## 致谢

- [Qt](https://www.qt.io/) —— 跨平台应用与 UI 框架
-[Logisim-evolution](https://github.com/logisim-evolution/logisim-evolution) --- logisim的现代替代版本 也是它给我提供了设计本项目的思路
