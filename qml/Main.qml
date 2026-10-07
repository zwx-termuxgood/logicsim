import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LogicSim

ApplicationWindow {
    id: root
    visible: true
    title: "LogicSim"
    color: Theme.bg

    readonly property bool isAndroid: Qt.platform.os === "android"
    width: isAndroid ? Screen.width : 1100
    height: isAndroid ? Screen.height : 640
    visibility: isAndroid ? Window.FullScreen : Window.Windowed
    minimumWidth: 500
    minimumHeight: 320

    property alias circuitRef: circuit
    readonly property bool circuitViewOnly: circuit.isViewOnly

    // ============ 全局状态 ============
    property var subList: []
    property var editCtxList: []
    property var recentList: []
    property bool forceQuit: false
    property bool saveBeforeExit: false

    // 性能缓存
    property var cachedComps: []
    property var cachedWires: []
    property var compById: ({})
    property var outPortsById: ({})
    property var inPortsById: ({})

    property real viewScale: 1.0
    property real viewX: 0
    property real viewY: 0

    property string placingType: ""
    property int placingBitWidth: 1
    property int placingInputCount: 2
    property string placingSubId: ""

    property string wiringFromComp: ""
    property int wiringFromPort: -1
    property bool wiring: false
    property bool wiringStartIsOutput: true
    property real wireEndX: 0
    property real wireEndY: 0

    property string selectedCompId: ""
    property var selectedComp: null
    property var selectedIds: []

    // 【新增】当前选中的子电路（用于属性面板显示）
    property string selectedSubId: ""

    property string statusText: "编辑模式"
    property string inputError: ""
    property string mode: "edit"
    property var errorList: []
    property var clipboardData: null

    property bool dirty: false
    property string currentFilePath: ""

    property bool boxSelecting: false
    property real boxStartX: 0
    property real boxStartY: 0
    property real boxCurrentX: 0
    property real boxCurrentY: 0

    property bool draggingSelection: false
    property real lastDragWorldX: 0
    property real lastDragWorldY: 0

    readonly property int windowHeight: height

    property alias subNameDialogRef: subNameDialog
    property alias closeConfirmRef: closeConfirm
    property alias exitConfirmRef: exitConfirm
    property alias confirmDialogRef: confirmDialog
    property alias statPopupRef: statPopup
    property alias recentPopupRef: recentPopup
    property alias contextPopupRef: contextPopup
    property alias saveDialogRef: saveDialog
    property alias openDialogRef: openDialog

    // ============ Circuit ============
    Circuit {
        id: circuit
        onChanged: {
            console.log("[QML] onChanged enter, ctxId=" + circuit.contextId
                        + " comps=" + circuit.components.length
                        + " selectedId=" + root.selectedCompId)
            root.rebuildCaches()
            console.log("[QML] rebuildCaches done")
            canvasView.requestPaint()
            root.refreshSel()
            console.log("[QML] refreshSel done")
            errorDebounce.restart()
            if (!root.forceQuit) {
                root.dirty = true
            }
            console.log("[QML] onChanged done")
        }
        onGeometryChanged: {
            var comps = circuit.components
            root.cachedComps = comps
            var m = root.compById
            for (var i = 0; i < comps.length; i++) m[comps[i].id] = comps[i]
            root.compById = m
            canvasView.requestPaint()
        }
        onContextChanged: {
            console.log("[QML] onContextChanged")
            canvasView.requestPaint()
            root.selectedCompId = ""
            root.selectedComp = null
            root.wiring = false
            root.placingType = ""
            root.placingSubId = ""
            root.selectedIds = []
            root.selectedSubId = ""     // 【新增】切换上下文时清空子电路选中
        }
        onSubcircuitsChanged: { root.subList = circuit.subcircuits }
        onEditContextsChanged: { root.editCtxList = circuit.editContexts }
        onErrorsChanged: { root.errorList = circuit.errors }
        onRecentFilesChanged: { root.recentList = circuit.recentFiles }
        onClockChanged: { canvasView.requestPaint() }
        onUndoRedoChanged: {
            console.log("[QML] onUndoRedoChanged canUndo=" + circuit.canUndo
                        + " canRedo=" + circuit.canRedo)
        }
    }

    readonly property bool clockRunning: circuit.clockRunning
    readonly property int clockFrequency: circuit.clockFrequency
    readonly property int clockTickCount: circuit.clockTickCount

    Timer {
        id: errorDebounce
        interval: 200
        onTriggered: circuit.refreshErrors()
    }

    function rebuildCaches() {
        console.log("[QML] rebuildCaches start")
        var comps = circuit.components
        var wrs = circuit.wires
        cachedComps = comps
        cachedWires = wrs
        var m = {}, op = {}, ip = {}
        for (var i = 0; i < comps.length; i++) {
            var c = comps[i]
            m[c.id] = c
            console.log("[QML]   comp " + i + " id=" + c.id + " type=" + c.type)
            op[c.id] = circuit.outputPortsInfo(c.id)
            ip[c.id] = circuit.inputPortsInfo(c.id)
        }
        compById = m
        outPortsById = op
        inPortsById = ip
        console.log("[QML] rebuildCaches done")
    }

    Timer {
        id: repaintTimer
        interval: 16; repeat: false
        onTriggered: canvasView.requestPaint()
    }
    function triggerRepaint() { repaintTimer.restart() }
    onWidthChanged: triggerRepaint()
    onHeightChanged: triggerRepaint()

    Component.onCompleted: {
        console.log("[QML] Component.onCompleted")
        subList = circuit.subcircuits
        editCtxList = circuit.editContexts
        errorList = circuit.errors
        recentList = circuit.recentFiles
        rebuildCaches()
        triggerRepaint()
        root.dirty = false
        console.log("[QML] Component.onCompleted done")
    }

    // ============ 快捷键 ============
    Shortcut {
        sequences: [StandardKey.Undo]
        context: Qt.WindowShortcut
        enabled: circuit.canUndo && !circuit.isViewOnly
        onActivated: {
            console.log("[QML] Shortcut Undo activated")
            circuit.undo()
        }
    }
    Shortcut {
        sequences: [StandardKey.Redo]
        context: Qt.WindowShortcut
        enabled: circuit.canRedo && !circuit.isViewOnly
        onActivated: {
            console.log("[QML] Shortcut Redo activated")
            circuit.redo()
        }
    }
    Shortcut {
        sequence: "Back"
        context: Qt.ApplicationShortcut
        enabled: !root.forceQuit
        onActivated: root.doExit()
    }

    // ============ 辅助函数 ============
    function refreshSel() {
        console.log("[QML] refreshSel selectedCompId=" + selectedCompId)
        selectedComp = (selectedCompId !== "") ? compById[selectedCompId] : null
        if (selectedComp && (selectedComp.type === "splitter" || selectedComp.type === "hub"))
            leftPanel.splitterSplitsInput.text = circuit.getSplitterSplitsStr(selectedCompId)
        if (selectedComp && selectedComp.type === "input") {
            leftPanel.inputValueField.text = circuit.getInputAsString(selectedCompId)
            inputError = ""
        }
        if (selectedComp && selectedComp.type === "text") {
            leftPanel.textContentField.text = selectedComp.content || ""
        }
        console.log("[QML] refreshSel done")
    }

    function resetView() {
        viewScale = 1.0; viewX = 0; viewY = 0
        triggerRepaint()
    }
    function zoomIn() {
        viewScale = Math.min(4.0, viewScale * 1.2)
        triggerRepaint()
    }
    function zoomOut() {
        viewScale = Math.max(0.15, viewScale / 1.2)
        triggerRepaint()
    }

    function fileUrlToLocal(url) {
        var s = url.toString()
        if (s.startsWith("file:///")) {
            s = s.substring(7)
            if (/^\/[A-Za-z]:/.test(s)) s = s.substring(1)
        } else if (s.startsWith("file://")) {
            s = s.substring(7)
        }
        return decodeURIComponent(s)
    }

    function confirmDelete(id) {
        if (circuit.isViewOnly) return
        confirmDialog.title = "删除元件"
        confirmDialog.message = "确定删除该元件及其所有连线吗？"
        confirmDialog.action = function() { circuit.removeComponent(id) }
        confirmDialog.open()
    }

    function quitApp() { root.forceQuit = true; Qt.quit() }

    function doSave() {
        if (currentFilePath === "") { saveDialog.open(); return }
        if (circuit.saveToFile(currentFilePath)) {
            dirty = false
            statusText = "已保存：" + currentFilePath
        } else {
            statusText = "保存失败：" + currentFilePath
        }
    }
    function doSaveAs() { saveDialog.open() }

    function doExit() {
        if (dirty) closeConfirm.open()
        else exitConfirm.open()
    }

    onClosing: function(close) {
        if (root.forceQuit) { close.accepted = true; return }
        close.accepted = false
        doExit()
    }

    // ============ 布局 ============
    TopBar {
        id: topBar
        main: root
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
    }

    ClockBar {
        id: clockBar
        main: root
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
    }

    Rectangle {
        id: readonlyBanner
        visible: circuit.isViewOnly
        anchors.top: clockBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.readonlyBannerH
        color: Theme.bgReadonly
        border.color: Theme.borderReadonly
        border.width: 1
        RowLayout {
            anchors.fill: parent
            anchors.margins: 5
            spacing: 8
            Text {
                text: "🔒 只读浏览：" + circuit.contextName + "（状态与父电路绑定）"
                color: Theme.accentOrange
                font.pixelSize: Theme.fsNormal
                Layout.fillWidth: true
            }
            Rectangle {
                Layout.preferredWidth: 80; Layout.preferredHeight: 22
                radius: Theme.smallRadius
                color: backView.pressed ? Theme.bgBackHi : Theme.bgBack
                Text {
                    anchors.centerIn: parent; text: "返回"
                    color: Theme.textWhite; font.pixelSize: Theme.fsSmall
                }
                MouseArea {
                    id: backView; anchors.fill: parent
                    onClicked: circuit.leaveSubcircuit()
                }
            }
        }
    }

    LeftPanel {
        id: leftPanel
        main: root
        anchors.top: readonlyBanner.visible ? readonlyBanner.bottom : clockBar.bottom
        anchors.left: parent.left
        anchors.bottom: parent.bottom
    }

    Item {
        id: canvasContainer
        anchors.top: readonlyBanner.visible ? readonlyBanner.bottom : clockBar.bottom
        anchors.left: leftPanel.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true

        CanvasView {
            id: canvasView
            main: root
            anchors.fill: parent
        }

        StatusBar {
            main: root
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }

    SubNameDialog { id: subNameDialog; main: root }
    CloseConfirmDialog { id: closeConfirm; main: root }
    ExitConfirmDialog { id: exitConfirm; main: root }
    ConfirmDialog { id: confirmDialog; main: root }
    StatsPopup { id: statPopup; main: root }
    RecentPopup { id: recentPopup; main: root; anchorItem: topBar.recentBtn }
    ContextPopup { id: contextPopup; main: root; anchorItem: topBar.ctxBtn }

    FileDialog {
        id: saveDialog
        title: "保存电路"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Logic JSON (*.json)"]
        defaultSuffix: "json"
        onAccepted: {
            var path = root.fileUrlToLocal(selectedFile)
            if (circuit.saveToFile(path)) {
                root.currentFilePath = path
                root.dirty = false
                root.statusText = "已保存：" + path
                if (root.saveBeforeExit) { root.saveBeforeExit = false; root.quitApp() }
            } else {
                root.statusText = "保存失败：" + path
                root.saveBeforeExit = false
            }
        }
        onRejected: root.saveBeforeExit = false
    }

    FileDialog {
        id: openDialog
        title: "打开电路"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Logic JSON (*.json)"]
        onAccepted: {
            var path = root.fileUrlToLocal(selectedFile)
            if (circuit.loadFromFile(path)) {
                root.currentFilePath = path
                root.dirty = false
                root.selectedCompId = ""
                root.selectedComp = null
                root.selectedIds = []
                root.selectedSubId = ""
                root.resetView()
                root.statusText = "已打开：" + path
            } else {
                root.statusText = "打开失败：" + path
            }
        }
    }
}