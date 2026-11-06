import QtQuick
// 引入 QtQuick 基础模块：Item/Text/Rectangle/MouseArea/Timer/Shortcut 等基础元素
import QtQuick.Controls
// 引入 Controls 模块：ApplicationWindow 在此
import QtQuick.Layouts
// 引入 Layouts 模块：RowLayout 布局在此
import QtQuick.Dialogs
// 引入 Dialogs 模块：FileDialog 文件对话框在此
import LogicSim
// 引入 C++ 侧注册的 LogicSim 模块：Circuit 类型与主题等在此

import "views/bars"
// 引入顶部栏、时钟栏、状态栏等子组件目录
import "views/canvas"
// 引入画布视图子组件目录
import "views/panels"
// 引入左侧面板、属性面板等子组件目录
import "views/dialogs"
// 引入各种对话框与弹窗子组件目录

ApplicationWindow {
    // 应用主窗口
    id: root
    // 窗口 id，供内部引用
    visible: true
    // 启动即显示
    title: "LogicSim"
    // 窗口标题
    color: Theme.bg
    // 窗口背景色取主题背景色

    readonly property bool isAndroid: Qt.platform.os === "android"
    // 是否运行在 Android 平台
    width: isAndroid ? Screen.width : 1100
    // 桌面宽度 1100，Android 用屏幕宽
    height: isAndroid ? Screen.height : 640
    // 桌面高度 640，Android 用屏幕高
    visibility: isAndroid ? Window.FullScreen : Window.Windowed
    // Android 全屏，桌面窗口化
    minimumWidth: 500
    // 最小宽度
    minimumHeight: 320
    // 最小高度

    property alias circuitRef: circuit
    // 把 Circuit 实例暴露给外部（各子组件通过 main.circuitRef 访问）
    readonly property bool circuitViewOnly: circuit.isViewOnly
    // 当前是否只读浏览模式

    property var subList: []
    // 子电路列表缓存
    property var editCtxList: []
    // 可编辑上下文列表缓存
    property var recentList: []
    // 最近文件列表缓存
    property bool forceQuit: false
    // 是否强制退出（跳过关闭确认）
    property bool saveBeforeExit: false
    // 是否处于"退出前保存"流程中

    property bool topCollapsed: false
    // 顶栏是否收起
    property bool leftCollapsed: false
    // 左侧面板是否收起
    property bool settingsOpen: false
    // 设置页是否打开

    property int visibleComponentCount: 0
    // 当前画布内可见元件数（由 CanvasView 回写）

    property var cachedComps: []
    // 元件列表缓存
    property var cachedWires: []
    // 连线列表缓存
    property var compById: ({})
    // 元件 id → 元件 map 缓存
    property var outPortsById: ({})
    // 元件 id → 输出端口几何缓存
    property var inPortsById: ({})
    // 元件 id → 输入端口几何缓存

    property var lastCompIds: []
    // 上次缓存时的元件 id 列表
    property var lastCompProps: ({})
    // 上次缓存时各元件影响尺寸的属性快照

    property real viewScale: 1.0
    // 视图缩放
    property real viewX: 0
    // 视图水平偏移
    property real viewY: 0
    // 视图垂直偏移

    property string placingType: ""
    // 正在放置的元件类型（空表示不在放置态）
    property int placingBitWidth: 1
    // 放置元件的位宽
    property int placingInputCount: 2
    // 放置元件的输入端口数
    property string placingSubId: ""
    // 正在放置的子电路 id（放置 sub 实例时使用）

    property string wiringFromComp: ""
    // 拉线起点元件 id
    property int wiringFromPort: -1
    // 拉线起点端口索引
    property bool wiring: false
    // 是否正在拉线
    property bool wiringStartIsOutput: true
    // 拉线起点是否为输出端口
    property real wireEndX: 0
    // 拉线预览终点屏幕 x
    property real wireEndY: 0
    // 拉线预览终点屏幕 y
    property real wiringStartX: 0
    // 拉线起点世界 x
    property real wiringStartY: 0
    // 拉线起点世界 y
    property int  wiringDirection: 0
    // 正交拉线拐角方向：0 未定，1 横优先，2 竖优先
    // ★ 新增：起点处命中的 wire 端点（缩短候选）
    property var shortenWireCandidates: []
    // 起点处可缩短的连线 id 列表

    property string selectedCompId: ""
    // 单选元件 id
    property var selectedComp: null
    // 单选元件 map
    property var selectedIds: []
    // 多选元件 id 列表
    property string selectedSubId: ""
    // 选中子电路 id
    property string selectedWireId: ""
    // 选中连线 id

    property string statusText: "编辑模式"
    // 状态栏文本
    property string inputError: ""
    // 输入值编辑错误消息
    property string mode: "edit"
    // 当前模式：edit/control/select
    property var errorList: []
    // 错误列表缓存
    property var clipboardData: null
    // 剪贴板数据（复制结果）

    property bool dirty: false
    // 是否有未保存修改
    property var  pendingAfterSave: null
    // 保存完成后要执行的回调（另存为流程用）
    property string currentFilePath: ""
    // 当前文件路径

    property bool boxSelecting: false
    // 是否正在框选
    property real boxStartX: 0
    // 框选起点屏幕 x
    property real boxStartY: 0
    // 框选起点屏幕 y
    property real boxCurrentX: 0
    // 框选当前屏幕 x
    property real boxCurrentY: 0
    // 框选当前屏幕 y

    property bool draggingSelection: false
    // 是否正在拖动选中集合
    property real lastDragWorldX: 0
    // 上次拖动世界 x
    property real lastDragWorldY: 0
    // 上次拖动世界 y

    readonly property int windowHeight: height
    // 窗口高度只读别名

    property alias subNameDialogRef: subNameDialog
    // 新建子电路对话框引用
    property alias closeConfirmRef: closeConfirm
    // 关闭确认对话框引用
    property alias exitConfirmRef: exitConfirm
    // 退出确认对话框引用
    property alias confirmDialogRef: confirmDialog
    // 通用确认对话框引用
    property alias unsavedDialogRef: unsavedDialog
    // 未保存对话框引用
    property alias statPopupRef: statPopup
    // 统计弹窗引用
    property alias recentPopupRef: recentPopup
    // 最近文件弹窗引用
    property alias contextPopupRef: contextPopup
    // 上下文选择弹窗引用
    property alias saveDialogRef: saveDialog
    // 保存文件对话框引用
    property alias openDialogRef: openDialog
    // 打开文件对话框引用
    property alias menuPopupRef: menuPopup
    // 主菜单弹窗引用

    readonly property string engineType: circuit.engineType
    // 当前引擎类型（转发自 Circuit）
    readonly property bool simulationRunning: circuit.simulationRunning
    // 仿真是否运行中（转发自 Circuit）

    Circuit {
        // 电路对象：整个应用的模型层
        id: circuit
        // 对象 id
        onChanged: {
            // 数据变化
            root.updateCachesValues()
            // 增量更新缓存
            canvasView.requestPaint()
            // 请求重绘画布
            root.refreshSel()
            // 刷新选中信息
            errorDebounce.restart()
            // 防抖刷新错误
            if (!root.forceQuit) root.dirty = true
            // 非强制退出时标记为已修改
        }
        onGeometryChanged: {
            // 几何变化（移动元件/改线）
            root.updateCachesValues()
            // 增量更新缓存
            canvasView.requestPaint()
            // 请求重绘
        }
        onStructureChanged: {
            // 结构变化（增删元件/连线）
            root.rebuildCachesFull()
            // 全量重建缓存
            canvasView.requestPaint()
            // 请求重绘
            root.refreshSel()
            // 刷新选中信息
        }
        onContextChanged: {
            // 上下文切换（进入/离开子电路等）
            canvasView.requestPaint()
            // 请求重绘
            root.selectedCompId = ""
            // 清空单选元件
            root.selectedComp = null
            // 清空单选元件引用
            root.wiring = false
            // 取消拉线
            root.placingType = ""
            // 取消放置
            root.placingSubId = ""
            // 清空待放置子电路
            root.selectedIds = []
            // 清空多选
            root.selectedSubId = ""
            // 清空选中子电路
            root.selectedWireId = ""
            // 清空选中连线
            root.wiringDirection = 0
            // 方向复位
            root.shortenWireCandidates = []
            // 清空缩短候选
            root.rebuildCachesFull()
            // 全量重建缓存
        }
        onSubcircuitsChanged: { root.subList = circuit.subcircuits }
        // 子电路列表变化时同步缓存
        onEditContextsChanged: { root.editCtxList = circuit.editContexts }
        // 可编辑上下文列表变化时同步缓存
        onErrorsChanged: { root.errorList = circuit.errors }
        // 错误列表变化时同步缓存
        onRecentFilesChanged: { root.recentList = circuit.recentFiles }
        // 最近文件变化时同步缓存
        onClockChanged: { canvasView.requestPaint() }
        // 时钟状态变化时重绘
        onSimulationTick: { canvasView.requestPaint() }
        // 仿真推进时重绘
        onSimulationChanged: { canvasView.requestPaint() }
        // 仿真启停时重绘
        onEngineChanged: { canvasView.requestPaint() }
        // 引擎切换时重绘
        onEngineSettingsChanged: { canvasView.requestPaint() }
        // 引擎设置变化时重绘
        onWireModeChanged: { canvasView.requestPaint() }
        // 连线模式变化时重绘
        onUndoRedoChanged: { }
        // 撤销/重做栈变化（当前无额外处理）
    }

    readonly property bool clockRunning: circuit.clockRunning
    // 时钟是否运行
    readonly property int clockFrequency: circuit.clockFrequency
    // 时钟频率
    readonly property int clockTickCount: circuit.clockTickCount
    // 时钟拍数

    Timer {
        // 错误刷新防抖定时器
        id: errorDebounce
        // 定时器 id
        interval: 200
        // 200ms 防抖
        onTriggered: circuit.refreshErrors()
        // 到时刷新错误列表
    }

    // 定义"增量更新缓存"函数：仅更新元件 map 与列表
    function updateCachesValues() {
        // 取元件与连线列表
        var comps = circuit.components
        var wrs = circuit.wires
        // 写入缓存
        cachedComps = comps
        cachedWires = wrs
        // 复用现有 map 增量更新
        var m = compById
        // 逐个元件更新
        for (var i = 0; i < comps.length; i++) {
            m[comps[i].id] = comps[i]
        }
        // 写回（触发属性通知）
        compById = m
    }

    // 定义"全量重建缓存"函数：重建元件 map、端口几何与属性快照
    function rebuildCachesFull() {
        // 结构变化时清空尺寸缓存
        canvasView.clearSizeCache()
        // 取元件与连线列表
        var comps = circuit.components
        var wrs = circuit.wires
        // 写入缓存
        cachedComps = comps
        cachedWires = wrs
        // 一次性取所有端口几何
        var allPorts = circuit.allPortsInfo()
        // 准备新缓存
        var m = {}, op = {}, ip = {}
        var ids = []
        var props = {}
        // 逐个元件填充
        for (var i = 0; i < comps.length; i++) {
            var c = comps[i]
            // 元件 map
            m[c.id] = c
            // 该元件端口几何
            var p = allPorts[c.id]
            // 输出端口
            op[c.id] = p ? (p.out || []) : []
            // 输入端口
            ip[c.id] = p ? (p.in  || []) : []
            // 记录 id
            ids.push(c.id)
            // 记录影响尺寸的属性快照
            props[c.id] = {
                bitWidth: c.bitWidth,
                inputCount: c.inputCount,
                rotation: c.rotation,
                splitsLen: c.outputSplits ? c.outputSplits.length : 0
            }
        }
        // 写回各缓存
        compById = m
        outPortsById = op
        inPortsById = ip
        lastCompIds = ids
        lastCompProps = props
    }

    Timer {
        // 重绘节流定时器
        id: repaintTimer
        // 定时器 id
        interval: 16; repeat: false
        // 16ms 单次
        onTriggered: canvasView.requestPaint()
        // 到时请求重绘
    }
    // 定义"触发重绘"函数：通过节流定时器合并多次请求
    function triggerRepaint() { repaintTimer.restart() }
    onWidthChanged: triggerRepaint()
    // 窗口宽变化时重绘
    onHeightChanged: triggerRepaint()
    // 窗口高变化时重绘

    Component.onCompleted: {
        // 组件加载完成初始化
        subList = circuit.subcircuits
        // 初始化子电路列表
        editCtxList = circuit.editContexts
        // 初始化可编辑上下文列表
        errorList = circuit.errors
        // 初始化错误列表
        recentList = circuit.recentFiles
        // 初始化最近文件列表
        rebuildCachesFull()
        // 全量重建缓存
        triggerRepaint()
        // 触发首次重绘
        root.dirty = false
        // 初始状态无修改
    }

    Shortcut {
        // 撤销快捷键
        sequences: [StandardKey.Undo]
        // 标准撤销快捷键
        context: Qt.WindowShortcut
        // 窗口级快捷键
        enabled: circuit.canUndo && !circuit.isViewOnly
        // 可撤销且非只读时启用
        onActivated: circuit.undo()
        // 触发撤销
    }
    Shortcut {
        // 重做快捷键
        sequences: [StandardKey.Redo]
        // 标准重做快捷键
        context: Qt.WindowShortcut
        // 窗口级快捷键
        enabled: circuit.canRedo && !circuit.isViewOnly
        // 可重做且非只读时启用
        onActivated: circuit.redo()
        // 触发重做
    }
    Shortcut {
        // 返回键：触发退出流程（Android 常用）
        sequence: "Back"
        // 返回键
        context: Qt.ApplicationShortcut
        // 应用级快捷键
        enabled: !root.forceQuit
        // 未强制退出时启用
        onActivated: root.doExit()
        // 触发退出流程
    }
    Shortcut {
        // 删除快捷键
        sequences: [StandardKey.Delete]
        // 标准删除键
        context: Qt.WindowShortcut
        // 窗口级快捷键
        enabled: !circuit.isViewOnly
        // 非只读时启用
        onActivated: {
            // 触发删除
            if (selectedWireId !== "") {
                // 有选中连线则删连线
                circuit.removeWire(selectedWireId)
                // 删除该连线
                selectedWireId = ""
                // 清空选中连线
            } else if (selectedCompId !== "") {
                // 有选中元件则弹确认后删除
                confirmDelete(selectedCompId)
                // 弹确认框删除元件
            }
        }
    }
    Shortcut {
        // 复制快捷键
        sequences: [StandardKey.Copy]
        // 标准复制键
        context: Qt.WindowShortcut
        // 窗口级快捷键
        enabled: !circuit.isViewOnly
        // 非只读时启用
        onActivated: {
            // 触发复制
            var ids = []
            // 待复制 id 列表
            if (selectedIds.length > 0) {
                // 优先多选
                for (var i = 0; i < selectedIds.length; i++) ids.push(selectedIds[i])
                // 收集多选 id
            } else if (selectedCompId !== "") {
                // 其次单选元件
                ids.push(selectedCompId)
                // 收集单选 id
            } else if (selectedWireId !== "") {
                // 最后选中连线
                ids.push(selectedWireId)
                // 收集连线 id
            }
            if (ids.length > 0) {
                // 有内容才复制
                clipboardData = circuit.copySelection(ids)
                // 复制到剪贴板缓存
                statusText = "已复制 " + ids.length + " 项"
                // 状态栏提示
            }
        }
    }
    Shortcut {
        // 粘贴快捷键
        sequences: [StandardKey.Paste]
        // 标准粘贴键
        context: Qt.WindowShortcut
        // 窗口级快捷键
        enabled: !circuit.isViewOnly && clipboardData !== null
        // 非只读且有剪贴板内容时启用
        onActivated: {
            // 触发粘贴
            if (clipboardData === null) return
            // 无剪贴板内容则忽略
            var newIds = circuit.pasteSelection(clipboardData, 30, 30)
            // 粘贴并偏移 30,30
            selectedIds = newIds
            // 选中新粘贴的元件
            statusText = "已粘贴 " + newIds.length + " 项"
            // 状态栏提示
        }
    }
    Shortcut {
        // ★ 全选快捷键：Ctrl+A / Cmd+A，按 StandardKey.SelectAll 匹配各平台习惯
        sequences: [StandardKey.SelectAll]
        // 标准全选键
        context: Qt.WindowShortcut
        // 窗口级快捷键
        enabled: !circuit.isViewOnly
        // 非只读时启用；只读浏览模式下不允许批量操作
        onActivated: root.doSelectAll()
        // 触发全选
    }

    // 定义"全选当前上下文所有元件"函数
    // 说明：
    //   · 只选择元件（含子电路实例），不选连线——连线的多选在现有 UI 里
    //     没有独立的集合，用 selectedWireId 单条操作更简单；
    //   · 同时清空 selectedCompId / selectedWireId / selectedSubId，
    //     保证"全选"这一动作不会与单选/连线/子电路的属性面板互相打架；
    //   · 走 selectedIds 这条路，是与框选多选（onReleased 里 boxSelecting
    //     分支）完全一致的机制，右键菜单/复制/删除等都已按 selectedIds
    //     处理，无需新增分支。
    function doSelectAll() {
        // 只读模式忽略
        if (circuit.isViewOnly) return
        // 收集当前上下文所有元件 id
        var ids = []
        // 取当前上下文元件列表
        var comps = circuit.components
        // 逐个收集
        for (var i = 0; i < comps.length; i++) ids.push(comps[i].id)
        // 写回多选集合
        selectedIds = ids
        // 清空单选元件
        selectedCompId = ""
        // 清空单选元件引用
        selectedComp = null
        // 清空选中连线
        selectedWireId = ""
        // 清空选中子电路
        selectedSubId = ""
        // 状态栏提示
        statusText = "已全选 " + ids.length + " 个元件"
        // 请求重绘（让画布上的选中高亮刷新）
        canvasView.requestPaint()
    }

    // 定义"刷新选中信息"函数：清理失效选中并同步属性面板
    function refreshSel() {
        // 选中元件已被删除则清空
        if (selectedCompId !== "" && !compById[selectedCompId]) {
            selectedCompId = ""
        }
        // 更新选中元件引用
        selectedComp = (selectedCompId !== "") ? (compById[selectedCompId] || null) : null

        // 分线器/集线器：同步分段输入框
        if (selectedComp && (selectedComp.type === "splitter" || selectedComp.type === "hub")) {
            // ★ 只在分段输入框没有编辑焦点时才同步：
            //   事件引擎运行时 onSimTimerTick 每 16ms 发一次 changed，
            //   本函数会被反复调用；若无条件覆盖 text，会把用户正在
            //   键入的分段字符串冲掉，表现为属性面板里改分段改不动。
            if (!leftPanel.splitterSplitsInput.activeFocus) {
                leftPanel.splitterSplitsInput.text = circuit.getSplitterSplitsStr(selectedCompId)
                // 同步分段方案字符串
            }
        }
        // 输入/常量：同步值输入框并清空错误
        if (selectedComp && (selectedComp.type === "input" || selectedComp.type === "const")) {
            // ★ 只在值输入框没有编辑焦点时才同步：
            //   仿真每帧 emit changed → refreshSel 被反复调用 → 无条件
            //   覆盖 inputValueField.text 会在 16ms 内把用户键入的字符
            //   冲掉，onEditingFinished 提交的永远是"被还原后的旧值"，
            //   这就是"event 引擎运行时非二进制显示下属性面板改不了值"
            //   的根因。加上 activeFocus 判断后，编辑期间不再打断，
            //   用户回车/失焦提交成功后，下一帧再同步为规范化后的值。
            //   inputError 也挪进本分支：编辑期间保留错误提示（否则
            //   每帧清空让错误来不及显示），待用户失焦后再清空。
            if (!leftPanel.inputValueField.activeFocus) {
                leftPanel.inputValueField.text = circuit.getInputAsString(selectedCompId)
                // 同步格式化后的值
                inputError = ""
                // 清空错误
            }
        }
        // 文本元件：同步内容输入框
        if (selectedComp && selectedComp.type === "text") {
            // ★ 同上：只在内容输入框没有编辑焦点时同步，
            //   避免每帧 emit changed 冲掉用户正在编辑的文本。
            if (!leftPanel.textContentField.activeFocus) {
                leftPanel.textContentField.text = selectedComp.content || ""
                // 同步文本内容
            }
        }

        // 选中连线已被删除则清空
        if (selectedWireId !== "") {
            var w = circuit.getWire(selectedWireId)
            // 查连线
            if (!w || !w.id) {
                // 不存在则清空
                selectedWireId = ""
            }
        }
    }

    // 定义"重置视图"函数
    function resetView() {
        // 缩放复位为 1
        viewScale = 1.0; viewX = 0; viewY = 0
        // 偏移归零
        triggerRepaint()
        // 触发重绘
    }
    // 定义"放大视图"函数
    function zoomIn() {
        // 放大 1.2 倍，上限 4
        viewScale = Math.min(4.0, viewScale * 1.2)
        // 触发重绘
        triggerRepaint()
    }
    // 定义"缩小视图"函数
    function zoomOut() {
        // 缩小 1.2 倍，下限 0.15
        viewScale = Math.max(0.15, viewScale / 1.2)
        // 触发重绘
        triggerRepaint()
    }

    // 定义"文件 URL 转本地路径"函数
    function fileUrlToLocal(url) {
        // 转字符串
        var s = url.toString()
        // 处理 file:/// 前缀
        if (s.startsWith("file:///")) {
            // 去掉前缀
            s = s.substring(7)
            // Windows 盘符前多余斜杠去掉
            if (/^\/[A-Za-z]:/.test(s)) s = s.substring(1)
        // 处理 file:// 前缀
        } else if (s.startsWith("file://")) {
            // 去掉前缀
            s = s.substring(7)
        }
        try {
            // 尝试解码百分号转义
            return decodeURIComponent(s)
        } catch (e) {
            // 解码失败原样返回
            return s
        }
    }

    // 定义"打开重命名子电路对话框"函数：转调左面板
    function openRenameSubDialog(subId, name) {
        // 委托左面板打开重命名对话框
        leftPanel.openRenameSubDialog(subId, name)
    }

    // 定义"确认删除元件"函数：弹确认框
    function confirmDelete(id) {
        // 只读模式禁止删除
        if (circuit.isViewOnly) return
        // 设置确认框标题
        confirmDialog.title = "删除元件"
        // 设置确认框消息
        confirmDialog.message = "确定删除该元件及其所有连线吗？"
        // 设置确认后的动作
        confirmDialog.action = function() { circuit.removeComponent(id) }
        // 打开确认框
        confirmDialog.open()
    }

    // 定义"退出应用"函数
    function quitApp() { root.forceQuit = true; Qt.quit() }
    // 标记强制退出并退出应用

    // 定义"带未保存检查执行操作"函数
    function doOpenFile(action) {
        // 有修改则先弹未保存对话框
        if (dirty) {
            // 记录待执行动作
            unsavedDialog.pendingAction = action
            // 打开未保存对话框
            unsavedDialog.open()
        } else {
            // 无修改直接执行
            action()
        }
    }

    // 定义"保存"函数
    function doSave() {
        // 无路径则弹另存为
        if (currentFilePath === "") { saveDialog.open(); return }
        // 保存到当前路径
        if (circuit.saveToFile(currentFilePath)) {
            // 成功：清除脏标记
            dirty = false
            // 状态栏提示
            statusText = "已保存：" + currentFilePath
        } else {
            // 失败：状态栏提示
            statusText = "保存失败：" + currentFilePath
        }
    }
    // 定义"另存为"函数：弹保存对话框
    function doSaveAs() { saveDialog.open() }

    // 定义"退出流程"函数：按是否有修改选择确认框
    function doExit() {
        // 有修改弹未保存确认
        if (dirty) closeConfirm.open()
        // 无修改弹退出确认
        else exitConfirm.open()
    }

    onClosing: function(close) {
        // 窗口关闭请求处理
        if (root.forceQuit) { close.accepted = true; return }
        // 强制退出则直接接受
        close.accepted = false
        // 否则先拒绝，走退出确认流程
        doExit()
        // 触发退出流程
    }

    Item {
        // 主区域容器
        id: mainArea
        // 容器 id
        anchors.fill: parent
        // 填满窗口

        Item {
            // 顶部区域容器
            id: topArea
            // 容器 id
            anchors.top: parent.top
            // 顶部对齐
            anchors.left: parent.left
            // 左侧对齐
            anchors.right: parent.right
            // 右侧对齐
            height: root.topCollapsed
                    ? Theme.collapsedBarH
                    : (Theme.topBarH + Theme.clockBarH)
            // 收起时只占收起条高度，否则顶栏+时钟栏高度
            z: 10
            // 层级较高

            TopBar {
                // 顶部工具栏
                id: topBar
                // 组件 id，供弹窗锚点引用
                main: root
                // 传递主窗口引用
                anchors.top: parent.top
                // 顶部对齐
                anchors.left: parent.left
                // 左侧对齐
                anchors.right: parent.right
                // 右侧对齐
                visible: !root.topCollapsed
                // 未收起时可见
            }

            ClockBar {
                // 时钟工具条
                id: clockBar
                // 组件 id
                main: root
                // 传递主窗口引用
                anchors.top: topBar.bottom
                // 顶部对齐顶栏底部
                anchors.left: parent.left
                // 左侧对齐
                anchors.right: parent.right
                // 右侧对齐
                visible: !root.topCollapsed
                // 未收起时可见
            }

            Rectangle {
                // 顶栏收起后的展开条
                id: expandTopBtn
                // 展开条 id
                visible: root.topCollapsed
                // 仅收起时可见
                anchors.left: parent.left
                // 左侧对齐
                anchors.right: parent.right
                // 右侧对齐
                anchors.top: parent.top
                // 顶部对齐
                height: Theme.collapsedBarH
                // 收起条高度
                color: Theme.bgBar
                // 背景色
                border.color: Theme.border
                // 边框色
                border.width: 1
                // 边框宽度

                Rectangle {
                    // 展开按钮
                    anchors.centerIn: parent
                    // 居中
                    width: 80; height: 24
                    // 尺寸
                    radius: Theme.smallRadius
                    // 圆角
                    color: expandTopMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                    // 颜色：按下高亮 > 普通
                    border.color: Theme.borderStrong; border.width: 1
                    // 边框
                    Text {
                        // 按钮文字
                        anchors.centerIn: parent
                        // 居中
                        text: "▼ 展开"
                        // 文案
                        color: Theme.text
                        // 文字色
                        font.pixelSize: Theme.fsSmall
                        // 小字号
                    }
                    MouseArea {
                        // 点击区
                        id: expandTopMa
                        // 点击区 id
                        anchors.fill: parent
                        // 填满按钮
                        onClicked: root.topCollapsed = false
                        // 点击展开顶栏
                    }
                }
            }
        }

        Rectangle {
            // 只读浏览提示条
            id: readonlyBanner
            // 提示条 id
            visible: circuit.isViewOnly
            // 仅只读浏览时可见
            anchors.top: topArea.bottom
            // 顶部对齐顶栏底部
            anchors.left: parent.left
            // 左侧对齐
            anchors.right: parent.right
            // 右侧对齐
            height: Theme.readonlyBannerH
            // 高度
            color: Theme.bgReadonly
            // 只读背景色
            border.color: Theme.borderReadonly
            // 只读边框色
            border.width: 1
            // 边框宽度
            z: 9
            // 层级略低于顶栏
            RowLayout {
                // 提示条内容行
                anchors.fill: parent
                // 填满提示条
                anchors.margins: 5
                // 四周内边距
                spacing: 8
                // 子项间距
                Text {
                    // 提示文字
                    text: "🔒 只读浏览：" + circuit.contextName + "（状态与父电路绑定）"
                    // 文案拼接当前上下文名
                    color: Theme.accentOrange
                    // 橙色强调
                    font.pixelSize: Theme.fsNormal
                    // 常规字号
                    Layout.fillWidth: true
                    // 占满剩余宽度
                }
                Rectangle {
                    // 返回按钮
                    Layout.preferredWidth: 80; Layout.preferredHeight: 22
                    // 尺寸
                    radius: Theme.smallRadius
                    // 圆角
                    color: backView.pressed ? Theme.bgBackHi : Theme.bgBack
                    // 颜色：按下高亮 > 返回色
                    Text {
                        // 按钮文字
                        anchors.centerIn: parent; text: "返回"
                        // 居中文字"返回"
                        color: Theme.textWhite; font.pixelSize: Theme.fsSmall
                        // 白字与小字号
                    }
                    MouseArea {
                        // 点击区
                        id: backView; anchors.fill: parent
                        // 填满按钮
                        onClicked: circuit.leaveSubcircuit()
                        // 点击离开子电路浏览
                    }
                }
            }
        }

        Item {
            // 左侧面板区域容器
            id: leftArea
            // 容器 id
            anchors.top: readonlyBanner.visible ? readonlyBanner.bottom : topArea.bottom
            // 顶部对齐（考虑只读提示条）
            anchors.left: parent.left
            // 左侧对齐
            anchors.bottom: parent.bottom
            // 底部对齐
            width: root.leftCollapsed ? Theme.collapsedBarW : Theme.leftPanelW
            // 收起时窄，展开时正常宽

            LeftPanel {
                // 左侧面板
                id: leftPanel
                // 组件 id，供外部引用其输入框等
                main: root
                // 传递主窗口引用
                anchors.fill: parent
                // 填满容器
                visible: !root.leftCollapsed
                // 未收起时可见
            }

            Rectangle {
                // 左侧面板收起后的展开条
                visible: root.leftCollapsed
                // 仅收起时可见
                anchors.fill: parent
                // 填满容器
                color: Theme.bgPanel
                // 面板背景色
                border.color: Theme.border
                // 边框色
                border.width: 1
                // 边框宽度

                Rectangle {
                    // 展开按钮
                    anchors.centerIn: parent
                    // 居中
                    width: 24; height: 80
                    // 尺寸
                    radius: Theme.smallRadius
                    // 圆角
                    color: expandLeftMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                    // 颜色：按下高亮 > 普通
                    border.color: Theme.borderStrong; border.width: 1
                    // 边框
                    Text {
                        // 按钮文字
                        anchors.centerIn: parent
                        // 居中
                        text: "▶"
                        // 右箭头
                        color: Theme.text
                        // 文字色
                        font.pixelSize: 16
                        // 字号
                    }
                    MouseArea {
                        // 点击区
                        id: expandLeftMa
                        // 点击区 id
                        anchors.fill: parent
                        // 填满按钮
                        onClicked: root.leftCollapsed = false
                        // 点击展开左面板
                    }
                }
            }
        }

        Item {
            // 画布区域容器
            id: canvasContainer
            // 容器 id
            anchors.top: readonlyBanner.visible ? readonlyBanner.bottom : topArea.bottom
            // 顶部对齐（考虑只读提示条）
            anchors.left: leftArea.right
            // 左侧对齐左面板右缘
            anchors.right: parent.right
            // 右侧对齐
            anchors.bottom: parent.bottom
            // 底部对齐
            clip: true
            // 裁剪超出部分

            CanvasView {
                // 画布视图
                id: canvasView
                // 组件 id，供外部请求重绘
                main: root
                // 传递主窗口引用
                anchors.fill: parent
                // 填满容器
            }

            StatusBar {
                // 状态栏
                main: root
                // 传递主窗口引用
                anchors.left: parent.left
                // 左侧对齐
                anchors.right: parent.right
                // 右侧对齐
                anchors.bottom: parent.bottom
                // 底部对齐
            }
        }
    }

    SubNameDialog { id: subNameDialog; main: root }
    // 新建子电路对话框实例
    CloseConfirmDialog { id: closeConfirm; main: root }
    // 关闭确认对话框实例
    ExitConfirmDialog { id: exitConfirm; main: root }
    // 退出确认对话框实例
    ConfirmDialog { id: confirmDialog; main: root }
    // 通用确认对话框实例
    UnsavedDialog { id: unsavedDialog; main: root }
    // 未保存对话框实例
    StatsPopup { id: statPopup; main: root }
    // 统计弹窗实例
    RecentPopup { id: recentPopup; main: root }
    // 最近文件弹窗实例
    ContextPopup { id: contextPopup; main: root; anchorItem: topBar.ctxBtn }
    // 上下文选择弹窗实例，锚点为顶栏上下文按钮
    MenuPopup { id: menuPopup; main: root; anchorItem: topBar.menuBtn }
    // 主菜单弹窗实例，锚点为顶栏菜单按钮

    SettingsPage {
        // 设置页实例
        id: settingsPage
        // 组件 id
        main: root
        // 传递主窗口引用
        visible: root.settingsOpen
        // 由 settingsOpen 控制显隐
        anchors.fill: parent
        // 覆盖整个窗口
        z: 2000
        // 层级很高
    }

    FileDialog {
        // 保存文件对话框
        id: saveDialog
        // 对话框 id，供外部引用
        title: "保存电路"
        // 标题
        fileMode: FileDialog.SaveFile
        // 保存文件模式
        nameFilters: ["Logic JSON (*.json)"]
        // 只显示/保存 JSON
        defaultSuffix: "json"
        // 默认后缀
        onAccepted: {
            // 确认保存
            var path = root.fileUrlToLocal(selectedFile)
            // URL 转本地路径
            if (circuit.saveToFile(path)) {
                // 保存成功
                root.currentFilePath = path
                // 更新当前路径
                root.dirty = false
                // 清除脏标记
                root.statusText = "已保存：" + path
                // 状态栏提示
                if (root.saveBeforeExit) { root.saveBeforeExit = false; root.quitApp() }
                // 退出前保存流程：保存后退出
                var act = root.pendingAfterSave
                // 取待执行回调
                root.pendingAfterSave = null
                // 清空回调
                if (act) act()
                // 有则执行
            } else {
                // 保存失败
                root.statusText = "保存失败：" + path
                // 状态栏提示
                root.saveBeforeExit = false
                // 清除退出前保存标记
            }
        }
        onRejected: root.saveBeforeExit = false
   // 取消时清除"退出前保存"标记
    }

    FileDialog {
        // 打开文件对话框
        id: openDialog
        // 对话框 id，供外部引用
        title: "打开电路"
        // 标题
        fileMode: FileDialog.OpenFile
        // 打开文件模式
        nameFilters: ["Logic JSON (*.json)"]
        // 只显示 JSON
        onAccepted: {
            // 确认打开
            var path = root.fileUrlToLocal(selectedFile)
            // URL 转本地路径
            if (circuit.loadFromFile(path)) {
                // 加载成功
                root.currentFilePath = path
                // 更新当前路径
                root.dirty = false
                // 清除脏标记
                root.selectedCompId = ""
                // 清空单选元件
                root.selectedComp = null
                // 清空单选元件引用
                root.selectedIds = []
                // 清空多选
                root.selectedSubId = ""
                // 清空选中子电路
                root.selectedWireId = ""
                // 清空选中连线
                root.resetView()
                // 重置视图
                root.statusText = "已打开：" + path
                // 状态栏提示
            } else {
                // 加载失败
                root.statusText = "打开失败：" + path
                // 状态栏提示失败
            }
        }
    }
}