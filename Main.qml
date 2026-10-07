import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LogicSim

ApplicationWindow {
    id: root
    visible: true
    title: "LogicSim"
    color: "#1a1a1a"

    readonly property bool isAndroid: Qt.platform.os === "android"
    width: isAndroid ? Screen.width : 1100
    height: isAndroid ? Screen.height : 640
    visibility: isAndroid ? Window.FullScreen : Window.Windowed
    minimumWidth: 500
    minimumHeight: 320

    property var subList: []
    property var editCtxList: []
    property var recentList: []
    property bool forceQuit: false
    property bool saveBeforeExit: false

    // ★ 性能缓存
    property var cachedComps: []
    property var cachedWires: []
    property var compById: ({})
    property var outPortsById: ({})
    property var inPortsById: ({})

    function rebuildCaches() {
        var comps = circuit.components
        var wrs = circuit.wires
        cachedComps = comps
        cachedWires = wrs
        var m = {}, op = {}, ip = {}
        for (var i = 0; i < comps.length; i++) {
            var c = comps[i]
            m[c.id] = c
            op[c.id] = circuit.outputPortsInfo(c.id)
            ip[c.id] = circuit.inputPortsInfo(c.id)
        }
        compById = m
        outPortsById = op
        inPortsById = ip
    }

    // ★ 时钟高频时防抖刷新错误
    Timer {
        id: errorDebounce
        interval: 200
        onTriggered: circuit.refreshErrors()
    }

    Circuit {
        id: circuit
        onChanged: {
            rebuildCaches()
            canvasArea.requestPaint()
            refreshSel()
            errorDebounce.restart()
        }
        onGeometryChanged: {
            var comps = circuit.components
            cachedComps = comps
            var m = compById
            for (var i = 0; i < comps.length; i++) m[comps[i].id] = comps[i]
            compById = m
            canvasArea.requestPaint()
        }
        onContextChanged: {
            canvasArea.requestPaint()
            root.selectedCompId = ""
            root.selectedComp = null
            root.wiring = false
            root.placingType = ""
            root.placingSubId = ""
            root.selectedIds = []
        }
        onSubcircuitsChanged: { root.subList = circuit.subcircuits }
        onEditContextsChanged: { root.editCtxList = circuit.editContexts }
        onErrorsChanged: { root.errorList = circuit.errors }
        onRecentFilesChanged: { root.recentList = circuit.recentFiles }
        onClockChanged: { canvasArea.requestPaint() }
    }

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

    Timer {
        id: repaintTimer
        interval: 16; repeat: false
        onTriggered: canvasArea.requestPaint()
    }
    function triggerRepaint() { repaintTimer.restart() }
    onWidthChanged: triggerRepaint()
    onHeightChanged: triggerRepaint()

    Component.onCompleted: {
        root.subList = circuit.subcircuits
        root.editCtxList = circuit.editContexts
        root.errorList = circuit.errors
        root.recentList = circuit.recentFiles
        rebuildCaches()
        triggerRepaint()
    }

    function refreshSel() {
        selectedComp = (selectedCompId !== "") ? compById[selectedCompId] : null
        if (selectedComp && (selectedComp.type === "splitter" || selectedComp.type === "hub"))
            splitterSplitsInput.text = circuit.getSplitterSplitsStr(selectedCompId)
        if (selectedComp && selectedComp.type === "input") {
            inputValueField.text = circuit.getInputAsString(selectedCompId)
            inputError = ""
        }
        if (selectedComp && selectedComp.type === "text") {
            textContentField.text = selectedComp.content || ""
        }
    }

    readonly property var elementGroups: [
        { name: "输入输出", items: [
            { type: "input", name: "输入开关" },
            { type: "output", name: "输出端口" },
            { type: "led", name: "LED" }
        ]},
        { name: "基本逻辑门", items: [
            { type: "and", name: "与门" },
            { type: "or", name: "或门" },
            { type: "not", name: "非门" }
        ]},
        { name: "复合逻辑门", items: [
            { type: "nand", name: "与非" },
            { type: "nor", name: "或非" },
            { type: "xor", name: "异或" },
            { type: "xnor", name: "同或" }
        ]},
        { name: "工具", items: [
            { type: "splitter", name: "分线器" },
            { type: "hub", name: "集线器" }
        ]},
        { name: "控制", items: [
            { type: "clock", name: "时钟" },
            { type: "text", name: "文字" }
        ]}
    ]

    function portPos(compId, portIdx, isOutput) {
        var c = compById[compId]
        if (!c) return { x: 0, y: 0 }
        var ports = isOutput ? outPortsById[compId] : inPortsById[compId]
        if (!ports || portIdx < 0 || portIdx >= ports.length) return { x: c.x, y: c.y }
        var p = ports[portIdx]
        return { x: c.x + p.x, y: c.y + p.y }
    }

    function compSize(comp) {
        var t = comp.type
        var bw = comp.bitWidth || 1
        if (t === "text") return { w: 140, h: 44 }
        if (t === "clock") return { w: 60, h: 40 }
        if (t === "led") return { w: Math.max(50, bw * 22 + 8), h: 40 }
        if (t === "input" || t === "output") return { w: Math.max(60, bw * 22 + 8), h: 40 }
        if (t === "splitter" || t === "hub") {
            var oc = (comp.outputSplits && comp.outputSplits.length) || bw
            return { w: 80, h: Math.max(60, oc * 24) }
        }
        if (t === "not") return { w: 80, h: 60 }
        if (t === "sub") {
            var ic = (comp.subInputNames || []).length
            var oc2 = (comp.subOutputNames || []).length
            return { w: 110, h: Math.max(60, Math.max(ic, oc2) * 24) }
        }
        var ic2 = comp.inputCount || 2
        return { w: 80, h: Math.max(60, ic2 * 24) }
    }

    function worldToScreen(wx, wy) { return { x: wx * viewScale + viewX, y: wy * viewScale + viewY } }
    function screenToWorld(sx, sy) { return { x: (sx - viewX) / viewScale, y: (sy - viewY) / viewScale } }
    function resetView() { viewScale = 1.0; viewX = 0; viewY = 0; triggerRepaint() }
    function zoomIn() { viewScale = Math.min(4.0, viewScale * 1.2); triggerRepaint() }
    function zoomOut() { viewScale = Math.max(0.15, viewScale / 1.2); triggerRepaint() }

    function isVisible(wx, wy, w, h) {
        var sx = (wx - w/2) * viewScale + viewX
        var sy = (wy - h/2) * viewScale + viewY
        var ex = sx + w * viewScale
        var ey = sy + h * viewScale
        return ex >= -50 && sx <= canvasArea.width + 50 &&
               ey >= -50 && sy <= canvasArea.height + 50
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

    function isGateType(t) {
        return t === "and" || t === "or" || t === "nand" ||
               t === "nor" || t === "xor" || t === "xnor"
    }
    function bitIndexAt(comp, worldX) {
        var size = compSize(comp)
        var left = comp.x - size.w / 2 + 4
        var i = Math.floor((worldX - left) / 22)
        var bw = comp.bitWidth || 1
        if (i < 0) i = 0
        if (i >= bw) i = bw - 1
        return i
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
        if (root.currentFilePath === "") { saveDialog.open(); return }
        if (circuit.saveToFile(root.currentFilePath)) {
            root.dirty = false
            root.statusText = "已保存：" + root.currentFilePath
        } else {
            root.statusText = "保存失败：" + root.currentFilePath
        }
    }
    function doSaveAs() { saveDialog.open() }

    function doExit() {
        if (root.dirty) closeConfirm.open()
        else exitConfirm.open()
    }

    Keys.onReleased: function(event) {
        if (event.key === Qt.Key_Back) { event.accepted = true; doExit() }
    }
    onClosing: function(close) {
        if (root.forceQuit) { close.accepted = true; return }
        close.accepted = false
        doExit()
    }

    // ============ 顶部工具栏 ============
    Rectangle {
        id: topBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 42; color: "#222222"; border.color: "#333333"; border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 6; anchors.rightMargin: 6
            anchors.topMargin: 5; anchors.bottomMargin: 5
            spacing: 3

            Text { text: "LogicSim"; color: "#eeeeee"; font.pixelSize: 14; font.bold: true }
            Rectangle { width: 1; height: 22; color: "#444444" }

            Repeater {
                model: [ { id: "edit", name: "编辑" },
                         { id: "control", name: "控制" },
                         { id: "select", name: "选择" } ]
                delegate: Rectangle {
                    Layout.preferredWidth: 44; Layout.preferredHeight: 28; radius: 5
                    color: root.mode === modelData.id ? "#4a4a4a" : "#2d2d2d"
                    border.color: root.mode === modelData.id ? "#cccccc" : "#383838"
                    border.width: 1
                    opacity: circuit.isViewOnly ? 0.5 : 1
                    Text { anchors.centerIn: parent; text: modelData.name
                        color: root.mode === modelData.id ? "#ffffff" : "#aaaaaa"; font.pixelSize: 11 }
                    MouseArea { anchors.fill: parent
                        enabled: !circuit.isViewOnly
                        onClicked: {
                            root.mode = modelData.id
                            root.wiring = false
                            root.placingType = ""
                            if (modelData.id !== "select") root.selectedIds = []
                        }
                    }
                }
            }

            Rectangle { width: 1; height: 22; color: "#444444" }

            Rectangle { Layout.preferredWidth: 42; Layout.preferredHeight: 28; radius: 5
                color: saveBtn.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#444444"; border.width: 1
                Text { anchors.centerIn: parent; text: "保存"; color: "#dddddd"; font.pixelSize: 11 }
                MouseArea { id: saveBtn; anchors.fill: parent; onClicked: root.doSave() } }
            Rectangle { Layout.preferredWidth: 54; Layout.preferredHeight: 28; radius: 5
                color: saveAsBtn.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#444444"; border.width: 1
                Text { anchors.centerIn: parent; text: "另存为"; color: "#dddddd"; font.pixelSize: 11 }
                MouseArea { id: saveAsBtn; anchors.fill: parent; onClicked: root.doSaveAs() } }
            Rectangle { Layout.preferredWidth: 42; Layout.preferredHeight: 28; radius: 5
                color: openBtn.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#444444"; border.width: 1
                Text { anchors.centerIn: parent; text: "打开"; color: "#dddddd"; font.pixelSize: 11 }
                MouseArea { id: openBtn; anchors.fill: parent; onClicked: openDialog.open() } }
            Rectangle { Layout.preferredWidth: 42; Layout.preferredHeight: 28; radius: 5
                color: recentBtn.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#444444"; border.width: 1
                Text { anchors.centerIn: parent; text: "最近"; color: "#dddddd"; font.pixelSize: 11 }
                MouseArea { id: recentBtn; anchors.fill: parent; onClicked: recentPopup.open() } }
            Rectangle { Layout.preferredWidth: 42; Layout.preferredHeight: 28; radius: 5
                color: statBtn.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#444444"; border.width: 1
                Text { anchors.centerIn: parent; text: "统计"; color: "#dddddd"; font.pixelSize: 11 }
                MouseArea { id: statBtn; anchors.fill: parent; onClicked: statPopup.open() } }
            Rectangle { Layout.preferredWidth: 42; Layout.preferredHeight: 28; radius: 5
                color: newBtn.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#444444"; border.width: 1
                Text { anchors.centerIn: parent; text: "新建"; color: "#dddddd"; font.pixelSize: 11 }
                MouseArea { id: newBtn; anchors.fill: parent; onClicked: {
                    confirmDialog.title = "新建电路"
                    confirmDialog.message = "确定清空并新建？"
                    confirmDialog.action = function() {
                        circuit.clear()
                        root.currentFilePath = ""
                        root.dirty = false
                    }
                    confirmDialog.open()
                }} }

            Rectangle { width: 1; height: 22; color: "#444444" }

            Rectangle { Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 5
                color: zo.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#444444"; border.width: 1
                Text { anchors.centerIn: parent; text: "−"; color: "#dddddd"; font.pixelSize: 15 }
                MouseArea { id: zo; anchors.fill: parent; onClicked: root.zoomOut() } }
            Rectangle { Layout.preferredWidth: 46; Layout.preferredHeight: 28; radius: 5
                color: rst.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#444444"; border.width: 1
                Text { anchors.centerIn: parent; text: Math.round(viewScale*100) + "%"
                    color: "#dddddd"; font.pixelSize: 10 }
                MouseArea { id: rst; anchors.fill: parent; onClicked: root.resetView() } }
            Rectangle { Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 5
                color: zi.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#444444"; border.width: 1
                Text { anchors.centerIn: parent; text: "+"; color: "#dddddd"; font.pixelSize: 15 }
                MouseArea { id: zi; anchors.fill: parent; onClicked: root.zoomIn() } }

            Item { Layout.fillWidth: true }

            Rectangle {
                id: ctxBtn
                Layout.minimumWidth: 120; Layout.preferredWidth: 150; Layout.maximumWidth: 180
                Layout.preferredHeight: 28; radius: 5
                color: ctxBtnMouse.pressed ? "#3a3a3a" : "#2d2d2d"
                border.color: "#555555"; border.width: 1
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 6; spacing: 4
                    Text { Layout.fillWidth: true; text: circuit.contextName
                        color: "#dddddd"; font.pixelSize: 11; elide: Text.ElideRight }
                    Text { text: "▼"; color: "#888888"; font.pixelSize: 9 }
                }
                MouseArea { id: ctxBtnMouse; anchors.fill: parent; onClicked: ctxPopup.open() }
            }

            Text { visible: root.dirty; text: "●"; color: "#ffb74d"; font.pixelSize: 14 }
        }
    }

    // ============ 时钟控制栏 ============
    Rectangle {
        id: clockBar
        anchors.top: topBar.bottom; anchors.left: parent.left; anchors.right: parent.right
        height: 36; color: "#1e1e1e"; border.color: "#333333"; border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 6

            Rectangle {
                Layout.preferredWidth: 80; Layout.preferredHeight: 26; radius: 4
                color: playBtn.pressed ? "#3a5a3a" : (circuit.clockRunning ? "#2e5e2e" : "#2d2d2d")
                border.color: circuit.clockRunning ? "#4caf50" : "#444444"; border.width: 1
                Text { anchors.centerIn: parent
                    text: circuit.clockRunning ? "⏸ 停止" : "▶ 运行"
                    color: "#dddddd"; font.pixelSize: 11 }
                MouseArea { id: playBtn; anchors.fill: parent
                    onClicked: {
                        if (circuit.clockRunning) circuit.stopClock()
                        else circuit.startClock()
                    } }
            }

            Rectangle {
                Layout.preferredWidth: 60; Layout.preferredHeight: 26; radius: 4
                color: stepBtn.pressed ? "#3a3a3a" : "#2d2d2d"
                border.color: "#444444"; border.width: 1
                Text { anchors.centerIn: parent; text: "单步"; color: "#dddddd"; font.pixelSize: 11 }
                MouseArea { id: stepBtn; anchors.fill: parent
                    onClicked: circuit.singleStep() }
            }

            Rectangle {
                Layout.preferredWidth: 60; Layout.preferredHeight: 26; radius: 4
                color: resetBtn.pressed ? "#5a1a1a" : "#3a1a1a"
                border.color: "#8a3030"; border.width: 1
                Text { anchors.centerIn: parent; text: "复位"; color: "#ffffff"; font.pixelSize: 11 }
                MouseArea { id: resetBtn; anchors.fill: parent
                    onClicked: {
                        circuit.stopClock()
                        circuit.resetAllInputs()
                    } }
            }

            Rectangle { width: 1; height: 22; color: "#444444" }

            Text { text: "频率"; color: "#aaaaaa"; font.pixelSize: 11 }
            SpinBox {
                id: freqSpin
                from: 1; to: 1000; editable: true
                Layout.preferredWidth: 100
                value: circuit.clockFrequency
                onValueChanged: circuit.setClockFrequency(value)
            }
            Text { text: "Hz"; color: "#aaaaaa"; font.pixelSize: 11 }

            Rectangle { width: 1; height: 22; color: "#444444" }

            Text { text: "拍数：" + circuit.clockTickCount; color: "#dddddd"; font.pixelSize: 11 }

            Item { Layout.fillWidth: true }
        }
    }

    Popup {
        id: ctxPopup
        x: Math.max(10, ctxBtn.mapToItem(null, 0, 0).x + ctxBtn.width - 240)
        y: ctxBtn.mapToItem(null, 0, ctxBtn.height).y
        width: 240
        height: Math.min(320, 36 * Math.max(1, root.editCtxList.length) + 16)
        modal: true; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#2a2a2a"; border.color: "#555555"; border.width: 1; radius: 8 }
        contentItem: Column {
            spacing: 0
            Repeater {
                model: root.editCtxList
                delegate: Rectangle {
                    width: ctxPopup.width; height: 36
                    color: (circuit.contextId === modelData.id && !circuit.isViewOnly)
                           ? "#3a3a3a" : (ma.pressed ? "#3a3a3a" : "transparent")
                    Text { anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.right: parent.right; anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.name + (modelData.id === "" ? "" : "  (子电路)")
                        color: (circuit.contextId === modelData.id && !circuit.isViewOnly)
                               ? "#ffffff" : "#dddddd"
                        font.pixelSize: 12; elide: Text.ElideRight }
                    MouseArea { id: ma; anchors.fill: parent
                        onClicked: { circuit.switchEditContext(modelData.id); ctxPopup.close() } }
                }
            }
        }
    }

    Popup {
        id: recentPopup
        x: Math.max(10, recentBtn.mapToItem(null, 0, 0).x - 100)
        y: recentBtn.mapToItem(null, 0, recentBtn.height).y
        width: 340
        height: Math.min(400, 44 * Math.max(1, root.recentList.length) + 70)
        modal: true; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#2a2a2a"; border.color: "#555555"; border.width: 1; radius: 8 }
        contentItem: Column {
            spacing: 0
            Rectangle {
                width: recentPopup.width; height: 32; color: "#222222"
                Text { anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "最近打开"; color: "#aaaaaa"; font.pixelSize: 12; font.bold: true }
                Rectangle {
                    anchors.right: parent.right; anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 60; height: 22; radius: 4
                    color: clearRecentMa.pressed ? "#7a2020" : "#5a1a1a"
                    border.color: "#8a3030"; border.width: 1
                    Text { anchors.centerIn: parent; text: "清空"; color: "#ffffff"; font.pixelSize: 10 }
                    MouseArea { id: clearRecentMa; anchors.fill: parent
                        enabled: root.recentList.length > 0
                        onClicked: circuit.clearRecentFiles() }
                }
            }
            Text { visible: root.recentList.length === 0
                width: recentPopup.width; height: 60
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: "暂无最近打开的文件"; color: "#666666"; font.pixelSize: 12 }
            Repeater {
                model: root.recentList
                delegate: Rectangle {
                    width: recentPopup.width; height: 44
                    color: itemMa.pressed ? "#3a3a3a" : "transparent"
                    Column {
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.right: delMaBox.left; anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter; spacing: 2
                        Text { text: modelData.name; color: "#dddddd"; font.pixelSize: 12
                            elide: Text.ElideRight; width: parent.width }
                        Text { text: modelData.time + "  " + modelData.path
                            color: "#777777"; font.pixelSize: 10
                            elide: Text.ElideMiddle; width: parent.width }
                    }
                    Rectangle { id: delMaBox
                        anchors.right: parent.right; anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24; height: 24; radius: 4
                        color: delMaMouse.pressed ? "#7a2020" : "#5a1a1a"
                        Text { anchors.centerIn: parent; text: "×"; color: "#ffffff"; font.pixelSize: 14 }
                        MouseArea { id: delMaMouse; anchors.fill: parent
                            onClicked: circuit.removeRecentFile(modelData.path) } }
                    MouseArea { id: itemMa
                        anchors.fill: parent; anchors.rightMargin: 40
                        onClicked: {
                            if (circuit.loadFromFile(modelData.path)) {
                                root.currentFilePath = modelData.path
                                root.dirty = false
                                root.selectedCompId = ""
                                root.selectedComp = null
                                root.selectedIds = []
                                root.resetView()
                                root.statusText = "已打开：" + modelData.name
                            } else {
                                root.statusText = "打开失败：" + modelData.path
                            }
                            recentPopup.close()
                        } }
                }
            }
            Item { width: 1; height: 8 }
        }
    }

    // ============ 统计弹窗 ============
    Popup {
        id: statPopup
        anchors.centerIn: parent
        width: 340
        height: 440
        modal: true; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#2a2a2a"; border.color: "#555555"; border.width: 1; radius: 8 }
        contentItem: ColumnLayout {
            spacing: 0
            Rectangle {
                Layout.fillWidth: true; height: 34; color: "#222222"
                Text {
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "电路统计（递归含子电路）"
                    color: "#ffffff"; font.pixelSize: 13; font.bold: true
                }
            }
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: statCol.height + 20
                Column {
                    id: statCol
                    x: 12; y: 6; width: parent.width - 24
                    spacing: 5
                    Repeater {
                        model: statModel
                        delegate: RowLayout {
                            width: statCol.width
                            spacing: 10
                            Text { text: model.k; color: "#aaaaaa"; font.pixelSize: 11
                                Layout.fillWidth: true }
                            Text { text: model.v; color: "#dddddd"; font.pixelSize: 11; font.bold: true }
                        }
                    }
                }
            }
        }
        ListModel { id: statModel }
        onOpened: refreshStats()
        function refreshStats() {
            var s = circuit.statistics()
            statModel.clear()
            statModel.append({ k: "逻辑门总数", v: s.totalGates })
            statModel.append({ k: "  与门 AND", v: s.byType["and"] || 0 })
            statModel.append({ k: "  或门 OR", v: s.byType["or"] || 0 })
            statModel.append({ k: "  非门 NOT", v: s.byType["not"] || 0 })
            statModel.append({ k: "  与非 NAND", v: s.byType["nand"] || 0 })
            statModel.append({ k: "  或非 NOR", v: s.byType["nor"] || 0 })
            statModel.append({ k: "  异或 XOR", v: s.byType["xor"] || 0 })
            statModel.append({ k: "  同或 XNOR", v: s.byType["xnor"] || 0 })
            statModel.append({ k: "输入元件", v: s.totalInputs })
            statModel.append({ k: "输出 / LED", v: s.totalOutputs })
            statModel.append({ k: "分线器", v: s.totalSplits })
            statModel.append({ k: "集线器", v: s.totalHubs })
            statModel.append({ k: "时钟", v: s.totalClocks })
            statModel.append({ k: "文字", v: s.totalTexts })
            statModel.append({ k: "子电路实例", v: s.totalSubInst })
            statModel.append({ k: "子电路定义数", v: s.subCount })
        }
    }

    Rectangle {
        id: readonlyBanner
        visible: circuit.isViewOnly
        anchors.top: clockBar.bottom; anchors.left: parent.left; anchors.right: parent.right
        height: 30; color: "#3a2a10"; border.color: "#5a3f10"; border.width: 1
        RowLayout {
            anchors.fill: parent; anchors.margins: 5; spacing: 8
            Text { text: "🔒 只读浏览：" + circuit.contextName + "（状态与父电路绑定）"
                color: "#ffb74d"; font.pixelSize: 12; Layout.fillWidth: true }
            Rectangle { Layout.preferredWidth: 80; Layout.preferredHeight: 22; radius: 4
                color: backView.pressed ? "#5a5a5a" : "#4a4a4a"
                Text { anchors.centerIn: parent; text: "返回"; color: "#ffffff"; font.pixelSize: 11 }
                MouseArea { id: backView; anchors.fill: parent; onClicked: circuit.leaveSubcircuit() } }
        }
    }

    Rectangle {
        id: leftPanel
        anchors.top: readonlyBanner.visible ? readonlyBanner.bottom : clockBar.bottom
        anchors.left: parent.left; anchors.bottom: parent.bottom
        width: 200; color: "#222222"; border.color: "#333333"; border.width: 1

        ColumnLayout {
            anchors.fill: parent; spacing: 0

            Flickable {
                id: paletteFlick
                Layout.fillWidth: true
                Layout.preferredHeight: root.mode !== "control" && !circuit.isViewOnly
                                      ? Math.round(root.height * 0.5) : 0
                visible: root.mode !== "control" && !circuit.isViewOnly
                clip: true
                contentWidth: width
                contentHeight: paletteColumn.height + 12
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: paletteColumn
                    width: paletteFlick.width; spacing: 3
                    Item { width: 1; height: 4 }

                    Repeater {
                        model: root.elementGroups
                        delegate: Column {
                            width: paletteColumn.width; spacing: 2
                            Rectangle { width: parent.width; height: 22; color: "#2a2a2a"
                                Text { anchors.left: parent.left; anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.name
                                    color: "#888888"; font.pixelSize: 11; font.bold: true } }
                            Repeater {
                                model: modelData.items
                                delegate: Rectangle {
                                    width: parent.width - 10; x: 5; height: 28; radius: 4
                                    color: (root.placingType === modelData.type && root.placingSubId === "")
                                           ? "#3a3a3a" : (itemMa.pressed ? "#3a3a3a" : "#2d2d2d")
                                    border.color: (root.placingType === modelData.type && root.placingSubId === "")
                                                  ? "#cccccc" : "#383838"
                                    border.width: 1
                                    Text { anchors.left: parent.left; anchors.leftMargin: 10
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.name; color: "#dddddd"; font.pixelSize: 12 }
                                    MouseArea { id: itemMa; anchors.fill: parent
                                        onClicked: {
                                            if (root.placingType === modelData.type && root.placingSubId === "") {
                                                root.placingType = ""
                                            } else {
                                                root.placingType = modelData.type
                                                root.placingSubId = ""
                                                root.statusText = "已选择 " + modelData.name + "，点画布放置"
                                            } } }
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 22; color: "#2a2a2a"
                        Text { anchors.left: parent.left; anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: "子电路 (" + root.subList.length + ")"
                            color: "#a371f7"; font.pixelSize: 11; font.bold: true } }
                    Rectangle {
                        width: parent.width - 10; x: 5; height: 28; radius: 4
                        color: newSubMa.pressed ? "#3a3a3a" : "#2d2d2d"
                        border.color: "#5a3f8a"; border.width: 1
                        Text { anchors.centerIn: parent; text: "+ 新建子电路"
                            color: "#a371f7"; font.pixelSize: 11 }
                        MouseArea { id: newSubMa; anchors.fill: parent
                            onClicked: { subNameInput.text = "子电路"; subNameDialog.open() } }
                    }
                    Repeater {
                        model: root.subList
                        delegate: Rectangle {
                            width: parent.width - 10; x: 5; height: 32; radius: 4
                            color: (root.placingType === "sub" && root.placingSubId === modelData.id)
                                   ? "#3a3a3a" : (subItemMa.pressed ? "#3a3a3a" : "#2d2d2d")
                            border.color: (root.placingType === "sub" && root.placingSubId === modelData.id)
                                          ? "#a371f7" : "#383838"
                            border.width: 1
                            Text { anchors.left: parent.left; anchors.leftMargin: 10
                                anchors.right: parent.right; anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name + " [" + modelData.inCount + "/" + modelData.outCount + "]"
                                color: "#dddddd"; font.pixelSize: 11; elide: Text.ElideRight }
                            MouseArea { id: subItemMa; anchors.fill: parent
                                onClicked: {
                                    root.placingType = "sub"
                                    root.placingSubId = modelData.id
                                    root.statusText = "已选择 " + modelData.name + "，点画布放置"
                                } }
                        }
                    }
                    Item { width: 1; height: 8 }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#333333" }

            Flickable {
                id: propsFlick
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: propsColumn.height + 12
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: propsColumn
                    width: propsFlick.width; spacing: 5
                    Item { width: 1; height: 6 }

                    Text { x: 8
                        text: root.mode === "select"
                              ? ("已选中 " + root.selectedIds.length + " 个元件")
                              : "属性"
                        color: "#888888"; font.pixelSize: 11; font.bold: true }

                    Column {
                        visible: root.mode === "select"
                        x: 6; width: parent.width - 12; spacing: 4
                        Rectangle { width: parent.width; height: 30; radius: 4
                            color: copyBtn.pressed ? "#3a3a3a" : "#2d2d2d"
                            border.color: root.selectedIds.length > 0 ? "#58a6ff" : "#444444"
                            border.width: 1
                            opacity: root.selectedIds.length > 0 ? 1 : 0.5
                            Text { anchors.centerIn: parent; text: "复制 (" + root.selectedIds.length + ")"
                                color: "#dddddd"; font.pixelSize: 11 }
                            MouseArea { id: copyBtn; anchors.fill: parent
                                enabled: root.selectedIds.length > 0
                                onClicked: {
                                    root.clipboardData = circuit.copySelection(root.selectedIds)
                                    root.statusText = "已复制 " + root.selectedIds.length + " 个元件"
                                } } }
                        Rectangle { width: parent.width; height: 30; radius: 4
                            visible: root.clipboardData !== null
                            color: pasteBtn.pressed ? "#3a3a3a" : "#2d2d2d"
                            border.color: "#444444"; border.width: 1
                            Text { anchors.centerIn: parent; text: "粘贴"
                                color: "#dddddd"; font.pixelSize: 11 }
                            MouseArea { id: pasteBtn; anchors.fill: parent
                                onClicked: {
                                    var newIds = circuit.pasteSelection(root.clipboardData, 30, 30)
                                    root.selectedIds = newIds
                                    root.statusText = "已粘贴 " + newIds.length + " 个元件（已选中）"
                                } } }
                        Rectangle { width: parent.width; height: 30; radius: 4
                            visible: root.selectedIds.length > 0
                            color: da.pressed ? "#6e2020" : "#5a1a1a"
                            border.color: "#8a3030"; border.width: 1
                            Text { anchors.centerIn: parent; text: "删除选中"
                                color: "#ffffff"; font.pixelSize: 11 }
                            MouseArea { id: da; anchors.fill: parent
                                onClicked: {
                                    circuit.removeComponents(root.selectedIds)
                                    root.selectedIds = []
                                } } }
                    }

                    Text { visible: root.selectedComp === null && root.selectedIds.length === 0
                        x: 8; width: parent.width - 16
                        text: root.mode === "select"
                              ? "拖拽画布空白处框选，或点元件选中"
                              : "未选中元件"
                        color: "#666666"; font.pixelSize: 11; wrapMode: Text.Wrap }

                    Column {
                        visible: root.selectedComp !== null && root.mode !== "select"
                        x: 6; width: parent.width - 12; spacing: 5

                        Text { text: "ID：" + (root.selectedComp ? root.selectedComp.id : ""); color: "#dddddd"; font.pixelSize: 11 }
                        Text { text: "类型：" + (root.selectedComp ? root.selectedComp.type : ""); color: "#dddddd"; font.pixelSize: 11 }

                        Row { spacing: 4
                            Text { text: "名称"; color: "#aaaaaa"; font.pixelSize: 11; width: 36; anchors.verticalCenter: parent.verticalCenter }
                            Rectangle { width: parent.parent.width - 42; height: 26
                                color: "#1a1a1a"; border.color: "#444444"; border.width: 1; radius: 4
                                TextField {
                                    anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 5
                                    color: "#dddddd"; font.pixelSize: 11
                                    background: null
                                    text: root.selectedComp ? (root.selectedComp.name || "") : ""
                                    selectByMouse: true
                                    readOnly: circuit.isViewOnly
                                    onEditingFinished: {
                                        if (root.selectedCompId === "" || circuit.isViewOnly) return
                                        circuit.renameComponent(root.selectedCompId, text)
                                    } } } }

                        Row { spacing: 4
                            Text { text: "位宽"; color: "#aaaaaa"; font.pixelSize: 11; width: 36; anchors.verticalCenter: parent.verticalCenter }
                            SpinBox {
                                from: 1; to: 64; editable: true; width: 90
                                enabled: !circuit.isViewOnly
                                value: root.selectedComp ? (root.selectedComp.bitWidth || 1) : 1
                                onValueChanged: {
                                    if (circuit.isViewOnly) return
                                    if (root.selectedComp && root.selectedCompId !== "" &&
                                        root.selectedComp.bitWidth !== value)
                                        circuit.setComponentProp(root.selectedCompId, "bitWidth", value)
                                } } }

                        Row { visible: root.selectedComp && root.isGateType(root.selectedComp.type)
                            spacing: 4
                            Text { text: "输入"; color: "#aaaaaa"; font.pixelSize: 11; width: 36; anchors.verticalCenter: parent.verticalCenter }
                            SpinBox {
                                from: 2; to: 64; editable: true; width: 90
                                enabled: !circuit.isViewOnly
                                value: root.selectedComp ? (root.selectedComp.inputCount || 2) : 2
                                onValueChanged: {
                                    if (circuit.isViewOnly) return
                                    if (root.selectedComp && root.selectedCompId !== "" &&
                                        root.selectedComp.inputCount !== value)
                                        circuit.setComponentProp(root.selectedCompId, "inputCount", value)
                                } } }

                        ComboBox {
                            id: baseCombo
                            width: parent.width
                            visible: root.selectedComp && (root.selectedComp.type === "input" ||
                                                           root.selectedComp.type === "output" ||
                                                           root.selectedComp.type === "led")
                            enabled: !circuit.isViewOnly
                            model: {
                                var bw = root.selectedComp ? (root.selectedComp.bitWidth || 1) : 1
                                var arr = ["二进制", "无符号十进制", "有符号十进制", "十六进制"]
                                if (bw >= 32) arr.push("单精度浮点")
                                if (bw >= 64) arr.push("双精度浮点")
                                return arr
                            }
                            onActivated: {
                                if (root.selectedCompId === "" || circuit.isViewOnly) return
                                var bw = root.selectedComp.bitWidth || 1
                                var map = ["bin", "udec", "sdec", "hex"]
                                if (bw >= 32) map.push("f32")
                                if (bw >= 64) map.push("f64")
                                circuit.setInputBase(root.selectedCompId, map[currentIndex] || "bin")
                            }
                            Connections {
                                target: root
                                function onSelectedCompChanged() {
                                    if (!root.selectedComp) return
                                    var b = root.selectedComp.displayBase || "bin"
                                    var map = { "bin":0, "udec":1, "sdec":2, "hex":3, "f32":4, "f64":5 }
                                    baseCombo.currentIndex = map[b] !== undefined ? map[b] : 0
                                }
                            }
                        }

                        Column {
                            visible: root.selectedComp && root.selectedComp.type === "input" &&
                                     (root.selectedComp.displayBase || "bin") !== "bin"
                            spacing: 4; width: parent.width
                            Text { text: "输入值"; color: "#aaaaaa"; font.pixelSize: 11 }
                            Rectangle { width: parent.width; height: 26
                                color: "#1a1a1a"
                                border.color: inputError.length > 0 ? "#aa4040" : "#444444"
                                border.width: 1; radius: 4
                                TextField {
                                    id: inputValueField
                                    anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 5
                                    color: "#dddddd"; font.pixelSize: 11; font.family: "monospace"
                                    background: null; selectByMouse: true
                                    readOnly: circuit.isViewOnly
                                    onEditingFinished: {
                                        if (root.selectedCompId === "" || circuit.isViewOnly) return
                                        inputError = circuit.setInputFromString(root.selectedCompId, text)
                                    } } }
                            Text { visible: inputError.length > 0
                                text: "⚠ " + inputError
                                color: "#e05252"; font.pixelSize: 10
                                width: parent.width; wrapMode: Text.Wrap }
                        }

                        Flow {
                            width: parent.width; spacing: 3
                            visible: root.selectedComp && root.selectedComp.type === "input" &&
                                     (root.selectedComp.displayBase || "bin") === "bin"
                            Repeater {
                                model: {
                                    if (!root.selectedComp) return []
                                    var bw = root.selectedComp.bitWidth || 1
                                    var arr = []
                                    for (var i = 0; i < bw; ++i) arr.push(i)
                                    return arr
                                }
                                delegate: Rectangle {
                                    width: 22; height: 22; radius: 3
                                    color: {
                                        if (!root.selectedComp) return "#2d2d2d"
                                        var bits = root.selectedComp.inputBits || []
                                        return (bits[index] || false) ? "#4caf50" : "#2d2d2d"
                                    }
                                    border.color: "#555555"; border.width: 1
                                    opacity: circuit.isViewOnly ? 0.7 : 1
                                    Text { anchors.centerIn: parent
                                        text: {
                                            if (!root.selectedComp) return "0"
                                            var bits = root.selectedComp.inputBits || []
                                            return (bits[index] || false) ? "1" : "0"
                                        }
                                        color: "#ffffff"; font.pixelSize: 10 }
                                    MouseArea { anchors.fill: parent
                                        enabled: !circuit.isViewOnly
                                        onClicked: {
                                            if (root.selectedCompId === "") return
                                            var bits = root.selectedComp.inputBits || []
                                            circuit.setBitValue(root.selectedCompId, index, !(bits[index] || false))
                                        } }
                                }
                            }
                        }

                        Column {
                            visible: root.selectedComp &&
                                     (root.selectedComp.type === "splitter" ||
                                      root.selectedComp.type === "hub")
                            spacing: 4; width: parent.width
                            Text { text: "分段（逗号分隔，和 = 位宽）"
                                color: "#aaaaaa"; font.pixelSize: 11 }
                            Rectangle { width: parent.width; height: 26
                                color: "#1a1a1a"; border.color: "#444444"; border.width: 1; radius: 4
                                TextField {
                                    id: splitterSplitsInput
                                    anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 5
                                    color: "#dddddd"; font.pixelSize: 11; font.family: "monospace"
                                    background: null; selectByMouse: true
                                    readOnly: circuit.isViewOnly
                                    onEditingFinished: {
                                        if (root.selectedCompId === "" || circuit.isViewOnly) return
                                        circuit.setSplitterSplits(root.selectedCompId, text)
                                    } } }
                            Text {
                                text: root.selectedComp && root.selectedComp.type === "splitter"
                                      ? "分线器：1 输入 → N 输出"
                                      : "集线器：N 输入 → 1 输出"
                                color: "#777777"; font.pixelSize: 10
                                width: parent.width; wrapMode: Text.Wrap
                            }
                        }

                        Column {
                            visible: root.selectedComp && root.selectedComp.type === "text"
                            spacing: 4; width: parent.width
                            Text { text: "文字内容"; color: "#aaaaaa"; font.pixelSize: 11 }
                            Rectangle { width: parent.width; height: 26
                                color: "#1a1a1a"; border.color: "#444444"; border.width: 1; radius: 4
                                TextField {
                                    id: textContentField
                                    anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 5
                                    color: "#dddddd"; font.pixelSize: 11
                                    background: null; selectByMouse: true
                                    readOnly: circuit.isViewOnly
                                    onEditingFinished: {
                                        if (root.selectedCompId === "" || circuit.isViewOnly) return
                                        circuit.setTextContent(root.selectedCompId, text)
                                    } } }
                        }

                        Rectangle { width: parent.width; height: 30; radius: 4
                            visible: !circuit.isViewOnly
                            color: delMa.pressed ? "#6e2020" : "#5a1a1a"
                            border.color: "#8a3030"; border.width: 1
                            Text { anchors.centerIn: parent; text: "删除"; color: "#ffffff"; font.pixelSize: 11 }
                            MouseArea { id: delMa; anchors.fill: parent
                                onClicked: root.confirmDelete(root.selectedCompId) } }
                    }

                    Rectangle { visible: root.errorList.length > 0
                        x: 8; width: parent.width - 16; height: 1; color: "#333333" }
                    Text { visible: root.errorList.length > 0
                        x: 8
                        text: "错误 (" + root.errorList.length + ")"
                        color: "#e05252"; font.pixelSize: 11; font.bold: true }
                    Repeater {
                        model: root.errorList
                        delegate: Text { x: 8; width: propsColumn.width - 16
                            text: "⚠ " + modelData.msg
                            color: "#e05252"; font.pixelSize: 10; wrapMode: Text.Wrap }
                    }

                    Item { width: 1; height: 20 }
                }
            }
        }
    }

    Item {
        id: canvasContainer
        anchors.top: readonlyBanner.visible ? readonlyBanner.bottom : clockBar.bottom
        anchors.left: leftPanel.right; anchors.right: parent.right; anchors.bottom: parent.bottom
        clip: true

        Canvas {
            id: canvasArea
            anchors.fill: parent
            renderStrategy: Canvas.Immediate
            onPaint: root.paintCanvas()
        }

        Rectangle {
            id: selectionBox
            visible: root.boxSelecting
            x: Math.min(root.boxStartX, root.boxCurrentX)
            y: Math.min(root.boxStartY, root.boxCurrentY)
            width: Math.abs(root.boxCurrentX - root.boxStartX)
            height: Math.abs(root.boxCurrentY - root.boxStartY)
            color: "#2058a6ff"; border.color: "#58a6ff"; border.width: 1
            z: 100
        }

        MouseArea {
            id: inputArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true

            property real lastMouseX: 0
            property real lastMouseY: 0
            property bool panning: false
            property bool draggingComp: false
            property string dragCompId: ""

            onPressed: function(mouse) {
                lastMouseX = mouse.x; lastMouseY = mouse.y
                var wp = root.screenToWorld(mouse.x, mouse.y)
                var editable = !circuit.isViewOnly

                if (editable && root.mode === "edit" && root.placingType !== "") {
                    var subId = root.placingType === "sub" ? root.placingSubId : ""
                    var newId = circuit.addComponent(root.placingType, wp.x, wp.y,
                                                      root.placingBitWidth,
                                                      root.placingInputCount,
                                                      subId)
                    if (newId === "") {
                        root.statusText = "⚠ 无法放置：会造成子电路循环引用"
                    } else {
                        root.selectedCompId = newId
                        root.refreshSel()
                    }
                    root.placingType = ""
                    root.placingSubId = ""
                    return
                }

                var comps = cachedComps

                if (root.mode === "select") {
                    for (var i = comps.length - 1; i >= 0; i--) {
                        var c = comps[i]
                        var size = root.compSize(c)
                        if (Math.abs(wp.x - c.x) < size.w/2 && Math.abs(wp.y - c.y) < size.h/2) {
                            if (root.selectedIds.indexOf(c.id) < 0) root.selectedIds = [c.id]
                            root.draggingSelection = true
                            root.lastDragWorldX = wp.x
                            root.lastDragWorldY = wp.y
                            canvasArea.requestPaint()
                            return
                        }
                    }
                    root.selectedIds = []
                    root.boxSelecting = true
                    root.boxStartX = mouse.x; root.boxStartY = mouse.y
                    root.boxCurrentX = mouse.x; root.boxCurrentY = mouse.y
                    canvasArea.requestPaint()
                    return
                }

                if (editable && root.mode === "edit") {
                    for (var i2 = comps.length - 1; i2 >= 0; i2--) {
                        var c2 = comps[i2]
                        var outs = outPortsById[c2.id] || []
                        for (var k = 0; k < outs.length; k++) {
                            var p = outs[k]
                            var pp = root.worldToScreen(c2.x + p.x, c2.y + p.y)
                            var dx = mouse.x - pp.x, dy = mouse.y - pp.y
                            if (dx*dx + dy*dy < 900) {
                                root.wiringFromComp = c2.id
                                root.wiringFromPort = k
                                root.wiringStartIsOutput = true
                                root.wiring = true
                                root.wireEndX = mouse.x
                                root.wireEndY = mouse.y
                                canvasArea.requestPaint()
                                return
                            }
                        }
                    }
                    for (var i3 = comps.length - 1; i3 >= 0; i3--) {
                        var c3 = comps[i3]
                        var ins = inPortsById[c3.id] || []
                        for (var k3 = 0; k3 < ins.length; k3++) {
                            var p3 = ins[k3]
                            var pp3 = root.worldToScreen(c3.x + p3.x, c3.y + p3.y)
                            var dx3 = mouse.x - pp3.x, dy3 = mouse.y - pp3.y
                            if (dx3*dx3 + dy3*dy3 < 900) {
                                root.wiringFromComp = c3.id
                                root.wiringFromPort = k3
                                root.wiringStartIsOutput = false
                                root.wiring = true
                                root.wireEndX = mouse.x
                                root.wireEndY = mouse.y
                                canvasArea.requestPaint()
                                return
                            }
                        }
                    }
                }

                for (var i4 = comps.length - 1; i4 >= 0; i4--) {
                    var c4 = comps[i4]
                    var size4 = root.compSize(c4)
                    if (Math.abs(wp.x - c4.x) < size4.w/2 && Math.abs(wp.y - c4.y) < size4.h/2) {
                        root.selectedCompId = c4.id
                        root.refreshSel()
                        if (editable && root.mode === "control") {
                            if (c4.type === "input") {
                                var base = c4.displayBase || "bin"
                                if (base === "bin") {
                                    var bitIdx = root.bitIndexAt(c4, wp.x)
                                    var bits = c4.inputBits || []
                                    circuit.setBitValue(c4.id, bitIdx, !(bits[bitIdx] || false))
                                }
                            }
                        } else if (editable && root.mode === "edit") {
                            draggingComp = true
                            dragCompId = c4.id
                        }
                        return
                    }
                }

                root.selectedCompId = ""
                root.refreshSel()
                panning = true
            }

            onPositionChanged: function(mouse) {
                if (root.boxSelecting) {
                    root.boxCurrentX = mouse.x; root.boxCurrentY = mouse.y
                    return
                }
                if (root.draggingSelection) {
                    var wp = root.screenToWorld(mouse.x, mouse.y)
                    var dwx = wp.x - root.lastDragWorldX
                    var dwy = wp.y - root.lastDragWorldY
                    if (Math.abs(dwx) > 0.01 || Math.abs(dwy) > 0.01) {
                        circuit.moveSelection(root.selectedIds, dwx, dwy)
                        root.lastDragWorldX = wp.x
                        root.lastDragWorldY = wp.y
                    }
                    return
                }
                if (root.wiring) {
                    root.wireEndX = mouse.x; root.wireEndY = mouse.y
                    canvasArea.requestPaint()
                    return
                }
                if (draggingComp && dragCompId !== "" && !circuit.isViewOnly) {
                    var wp2 = root.screenToWorld(mouse.x, mouse.y)
                    circuit.moveComponent(dragCompId, wp2.x, wp2.y)
                    return
                }
                if (panning) {
                    root.viewX += mouse.x - lastMouseX
                    root.viewY += mouse.y - lastMouseY
                    lastMouseX = mouse.x; lastMouseY = mouse.y
                    canvasArea.requestPaint()
                }
            }

            onReleased: function(mouse) {
                if (root.boxSelecting) {
                    var x1 = Math.min(root.boxStartX, mouse.x)
                    var y1 = Math.min(root.boxStartY, mouse.y)
                    var x2 = Math.max(root.boxStartX, mouse.x)
                    var y2 = Math.max(root.boxStartY, mouse.y)
                    if (Math.abs(x2 - x1) < 5 && Math.abs(y2 - y1) < 5) {
                        root.selectedIds = []
                    } else {
                        var ids = []
                        var comps5 = cachedComps
                        for (var i = 0; i < comps5.length; i++) {
                            var c = comps5[i]
                            var sp = root.worldToScreen(c.x, c.y)
                            if (sp.x >= x1 && sp.x <= x2 && sp.y >= y1 && sp.y <= y2)
                                ids.push(c.id)
                        }
                        root.selectedIds = ids
                        root.statusText = "已选中 " + ids.length + " 个元件"
                    }
                    root.boxSelecting = false
                    canvasArea.requestPaint()
                }

                if (root.wiring) {
                    var comps = cachedComps
                    var hitTo = false

                    if (root.wiringStartIsOutput) {
                        for (var i = comps.length - 1; i >= 0 && !hitTo; i--) {
                            var c = comps[i]
                            var ins = inPortsById[c.id] || []
                            for (var k = 0; k < ins.length; k++) {
                                var p = ins[k]
                                var pp = root.worldToScreen(c.x + p.x, c.y + p.y)
                                var dx = mouse.x - pp.x, dy = mouse.y - pp.y
                                if (dx*dx + dy*dy < 900) {
                                    if (!(c.id === root.wiringFromComp && k === root.wiringFromPort)) {
                                        circuit.addWire(root.wiringFromComp, root.wiringFromPort, c.id, k)
                                        root.statusText = "已连线"
                                    }
                                    hitTo = true
                                    break
                                }
                            }
                        }
                    } else {
                        for (var i4b = comps.length - 1; i4b >= 0 && !hitTo; i4b--) {
                            var c4b = comps[i4b]
                            var outs = outPortsById[c4b.id] || []
                            for (var k4b = 0; k4b < outs.length; k4b++) {
                                var p4b = outs[k4b]
                                var pp4b = root.worldToScreen(c4b.x + p4b.x, c4b.y + p4b.y)
                                var dx4b = mouse.x - pp4b.x, dy4b = mouse.y - pp4b.y
                                if (dx4b*dx4b + dy4b*dy4b < 900) {
                                    if (!(c4b.id === root.wiringFromComp && k4b === root.wiringFromPort)) {
                                        circuit.addWire(c4b.id, k4b, root.wiringFromComp, root.wiringFromPort)
                                        root.statusText = "已连线"
                                    }
                                    hitTo = true
                                    break
                                }
                            }
                        }
                    }
                    root.wiring = false
                    root.wiringFromComp = ""
                    root.wiringFromPort = -1
                    canvasArea.requestPaint()
                }

                panning = false
                draggingComp = false
                dragCompId = ""
                root.draggingSelection = false
            }

            onCanceled: function() {
                panning = false; draggingComp = false; dragCompId = ""
                root.draggingSelection = false
                root.boxSelecting = false
                root.wiring = false
                root.wiringFromComp = ""
                root.wiringFromPort = -1
                canvasArea.requestPaint()
            }

            onWheel: function(wheel) {
                var oldScale = root.viewScale
                var factor = wheel.angleDelta.y > 0 ? 1.15 : 1/1.15
                var newScale = Math.max(0.15, Math.min(4.0, root.viewScale * factor))
                if (newScale === oldScale) return
                var mx = wheel.x, my = wheel.y
                root.viewX = mx - (mx - root.viewX) * (newScale / oldScale)
                root.viewY = my - (my - root.viewY) * (newScale / oldScale)
                root.viewScale = newScale
                triggerRepaint()
            }

            onDoubleClicked: function(mouse) {
                var wp = root.screenToWorld(mouse.x, mouse.y)
                var comps6 = cachedComps
                for (var i = comps6.length - 1; i >= 0; i--) {
                    var c = comps6[i]
                    var size = root.compSize(c)
                    if (Math.abs(wp.x - c.x) < size.w/2 && Math.abs(wp.y - c.y) < size.h/2) {
                        if (c.type === "sub") circuit.enterSubcircuit(c.subId)
                        return
                    }
                }
            }
        }

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 22; color: "#1a1a1a"
            Text {
                anchors.left: parent.left; anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: "[" + root.mode + "] " + root.statusText
                color: root.errorList.length > 0 ? "#e05252" : "#888888"
                font.pixelSize: 11; elide: Text.ElideRight
            }
        }
    }

    function paintCanvas() {
        var ctx = canvasArea.getContext("2d")
        var W = canvasArea.width, H = canvasArea.height
        if (W <= 0 || H <= 0) return

        ctx.clearRect(0, 0, W, H)
        ctx.fillStyle = "#1a1a1a"; ctx.fillRect(0, 0, W, H)

        var step = 40 * viewScale
        if (step >= 8) {
            var ox = viewX % step, oy = viewY % step
            ctx.strokeStyle = "#252525"; ctx.lineWidth = 1
            ctx.beginPath()
            for (var x = ox; x < W; x += step) { ctx.moveTo(x, 0); ctx.lineTo(x, H) }
            for (var y = oy; y < H; y += step) { ctx.moveTo(0, y); ctx.lineTo(W, y) }
            ctx.stroke()
        }

        var simplify = viewScale < 0.35

        var wires = cachedWires
        for (var i = 0; i < wires.length; i++) {
            var w = wires[i]
            var p1 = portPos(w.fromComp, w.fromPort, true)
            var p2 = portPos(w.toComp, w.toPort, false)
            var s1 = worldToScreen(p1.x, p1.y)
            var s2 = worldToScreen(p2.x, p2.y)

            if ((s1.x < -100 && s2.x < -100) || (s1.x > W+100 && s2.x > W+100)) continue
            if ((s1.y < -100 && s2.y < -100) || (s1.y > H+100 && s2.y > H+100)) continue

            var cx = Math.max(Math.abs(s2.x - s1.x) / 2, 40 * viewScale)
            var fromC = compById[w.fromComp]
            var wireVal = false
            if (fromC && fromC.outputPorts && fromC.outputPorts.length > w.fromPort) {
                var sv = fromC.outputPorts[w.fromPort]
                if (sv && sv.length > 0) wireVal = (sv.charAt(0) === '1')
            }
            ctx.strokeStyle = wireVal ? "#4caf50" : "#666666"
            ctx.lineWidth = simplify ? 1 : Math.max(1.5, 2.5 * viewScale)
            ctx.beginPath()
            ctx.moveTo(s1.x, s1.y)
            ctx.bezierCurveTo(s1.x + cx, s1.y, s2.x - cx, s2.y, s2.x, s2.y)
            ctx.stroke()
        }

        if (wiring) {
            var p = portPos(wiringFromComp, wiringFromPort, wiringStartIsOutput)
            var sp = worldToScreen(p.x, p.y)
            ctx.strokeStyle = "#ffb74d"; ctx.lineWidth = 2
            ctx.setLineDash([6, 6])
            ctx.beginPath()
            if (wiringStartIsOutput) {
                ctx.moveTo(sp.x, sp.y); ctx.lineTo(wireEndX, wireEndY)
            } else {
                ctx.moveTo(wireEndX, wireEndY); ctx.lineTo(sp.x, sp.y)
            }
            ctx.stroke()
            ctx.setLineDash([])
        }

        var comps = cachedComps
        for (var j = 0; j < comps.length; j++) {
            var comp = comps[j]
            var size = compSize(comp)
            if (!isVisible(comp.x, comp.y, size.w, size.h)) continue
            drawComponent(ctx, comp, simplify)
        }
    }

    function drawComponent(ctx, comp, simplify) {
        var t = comp.type
        var size = compSize(comp)
        var sp = worldToScreen(comp.x - size.w/2, comp.y - size.h/2)
        var w = size.w * viewScale
        var h = size.h * viewScale
        var isSel = (comp.id === selectedCompId)
        var isMultiSel = (selectedIds.indexOf(comp.id) >= 0)

        ctx.save()

        if (isMultiSel) {
            ctx.strokeStyle = "#58a6ff"; ctx.lineWidth = 2
            ctx.setLineDash([5, 3])
            ctx.strokeRect(sp.x - 3, sp.y - 3, w + 6, h + 6)
            ctx.setLineDash([])
        }

        if (t === "text") {
            if (!simplify) {
                ctx.fillStyle = "#dddddd"
                ctx.font = Math.max(10, Math.round(14 * viewScale)) + "px sans-serif"
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                ctx.fillText(comp.content || "文本", sp.x + w/2, sp.y + h/2)
            }
            if (isSel) {
                ctx.strokeStyle = "#58a6ff"; ctx.lineWidth = 1.5
                ctx.setLineDash([4, 3])
                ctx.strokeRect(sp.x, sp.y, w, h)
                ctx.setLineDash([])
            }
            ctx.restore()
            return
        }

        if (t === "clock") {
            ctx.fillStyle = "#2a3a2a"
            ctx.strokeStyle = isSel ? "#ffffff" : "#4caf50"
            ctx.lineWidth = isSel ? 3 : 2
            roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
            if (!simplify) {
                var cval = (comp.outputPorts && comp.outputPorts[0] === "1")
                ctx.fillStyle = cval ? "#4caf50" : "#555555"
                ctx.font = Math.max(10, Math.round(14 * viewScale)) + "px monospace"
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                ctx.fillText(cval ? "1" : "0", sp.x + w/2, sp.y + h/2 - 4*viewScale)
                ctx.strokeStyle = cval ? "#4caf50" : "#666666"
                ctx.lineWidth = 1.5 * viewScale
                ctx.beginPath()
                var waveY = sp.y + h - 8*viewScale
                var waveAmp = 4*viewScale
                ctx.moveTo(sp.x + 6*viewScale, waveY + waveAmp)
                ctx.lineTo(sp.x + w*0.3, waveY + waveAmp)
                ctx.lineTo(sp.x + w*0.3, waveY - waveAmp)
                ctx.lineTo(sp.x + w*0.7, waveY - waveAmp)
                ctx.lineTo(sp.x + w*0.7, waveY + waveAmp)
                ctx.lineTo(sp.x + w - 6*viewScale, waveY + waveAmp)
                ctx.stroke()
            }
            ctx.restore()
            return
        }

        if (t === "led") {
            ctx.fillStyle = "#2a2a2a"
            ctx.beginPath()
            ctx.arc(sp.x + w/2, sp.y + h/2, Math.min(w, h)/2, 0, Math.PI*2)
            ctx.fill()
            ctx.strokeStyle = isSel ? "#ffffff" : "#555555"
            ctx.lineWidth = isSel ? 3 : 2
            ctx.stroke()
        } else if (t === "input") {
            ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#666666"
            ctx.lineWidth = isSel ? 3 : 2
            roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
        } else if (t === "and" || t === "nand") {
            ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
            ctx.lineWidth = isSel ? 3 : 2
            ctx.beginPath()
            var r = h / 2
            ctx.moveTo(sp.x, sp.y); ctx.lineTo(sp.x + w/2, sp.y)
            ctx.arc(sp.x + w/2, sp.y + r, r, -Math.PI/2, Math.PI/2, false)
            ctx.lineTo(sp.x, sp.y + h); ctx.closePath(); ctx.fill(); ctx.stroke()
            if (t === "nand") {
                ctx.beginPath()
                ctx.arc(sp.x + w + 5*viewScale, sp.y + h/2, 5*viewScale, 0, Math.PI*2)
                ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
            }
        } else if (t === "or" || t === "nor" || t === "xor" || t === "xnor") {
            ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
            ctx.lineWidth = isSel ? 3 : 2
            ctx.beginPath()
            ctx.moveTo(sp.x, sp.y)
            ctx.quadraticCurveTo(sp.x + w*0.6, sp.y, sp.x + w, sp.y + h/2)
            ctx.quadraticCurveTo(sp.x + w*0.6, sp.y + h, sp.x, sp.y + h)
            ctx.quadraticCurveTo(sp.x + w*0.3, sp.y + h/2, sp.x, sp.y)
            ctx.closePath(); ctx.fill(); ctx.stroke()
            if (t === "xor" || t === "xnor") {
                ctx.beginPath()
                ctx.moveTo(sp.x - 6*viewScale, sp.y)
                ctx.quadraticCurveTo(sp.x - 6*viewScale + w*0.3, sp.y + h/2,
                                      sp.x - 6*viewScale, sp.y + h)
                ctx.stroke()
            }
            if (t === "nor" || t === "xnor") {
                ctx.beginPath()
                ctx.arc(sp.x + w + 5*viewScale, sp.y + h/2, 5*viewScale, 0, Math.PI*2)
                ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
            }
        } else if (t === "not") {
            ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
            ctx.lineWidth = isSel ? 3 : 2
            ctx.beginPath()
            ctx.moveTo(sp.x, sp.y); ctx.lineTo(sp.x, sp.y + h)
            ctx.lineTo(sp.x + w*0.85, sp.y + h/2); ctx.closePath(); ctx.fill(); ctx.stroke()
            ctx.beginPath()
            ctx.arc(sp.x + w*0.85 + 6*viewScale, sp.y + h/2, 6*viewScale, 0, Math.PI*2)
            ctx.fillStyle = "#2d2d2d"; ctx.fill(); ctx.stroke()
        } else if (t === "splitter" || t === "hub") {
            ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#999999"
            ctx.lineWidth = isSel ? 3 : 2
            roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
        } else if (t === "output") {
            ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#ff9800"
            ctx.lineWidth = isSel ? 3 : 2
            roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
        } else if (t === "sub") {
            ctx.fillStyle = "#1a2433"; ctx.strokeStyle = isSel ? "#ffffff" : "#a371f7"
            ctx.lineWidth = isSel ? 3 : 2
            roundRect(ctx, sp.x, sp.y, w, h, 6 * viewScale); ctx.fill(); ctx.stroke()
        } else {
            ctx.fillStyle = "#2d2d2d"; ctx.strokeStyle = isSel ? "#ffffff" : "#555555"
            ctx.lineWidth = isSel ? 3 : 2
            roundRect(ctx, sp.x, sp.y, w, h, 4 * viewScale); ctx.fill(); ctx.stroke()
        }

        if (simplify) {
            ctx.restore()
            return
        }

        var bw = comp.bitWidth || 1
        var base = comp.displayBase || "bin"

        if (t === "input") {
            var bits = comp.inputBits || []
            if (base === "bin") {
                for (var i = 0; i < bw; ++i) {
                    var gx = sp.x + (4 + i * 22) * viewScale
                    var gy = sp.y + 4 * viewScale
                    var gw = 20 * viewScale
                    var gh = h - 8 * viewScale
                    var v = bits[i] || false
                    ctx.fillStyle = v ? "#4caf50" : "#1a1a1a"
                    ctx.strokeStyle = "#555555"; ctx.lineWidth = 1
                    roundRect(ctx, gx, gy, gw, gh, 3 * viewScale); ctx.fill(); ctx.stroke()
                    ctx.fillStyle = v ? "#ffffff" : "#888888"
                    ctx.font = Math.max(8, Math.round(12 * viewScale)) + "px monospace"
                    ctx.textAlign = "center"; ctx.textBaseline = "middle"
                    ctx.fillText(v ? "1" : "0", gx + gw/2, gy + gh/2)
                }
            } else {
                var s = circuit.getInputAsString(comp.id)
                ctx.fillStyle = "#dddddd"
                ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                ctx.fillText(s, sp.x + w/2, sp.y + h/2)
            }
        } else if (t === "output") {
            var ovStr = ""
            if (comp.inputPortValues && comp.inputPortValues.length > 0)
                ovStr = comp.inputPortValues[0] || ""
            if (base === "bin") {
                for (var i2 = 0; i2 < bw; ++i2) {
                    var gx2 = sp.x + (4 + i2 * 22) * viewScale
                    var gy2 = sp.y + 4 * viewScale
                    var gw2 = 20 * viewScale
                    var gh2 = h - 8 * viewScale
                    var v2 = (i2 < ovStr.length && ovStr.charAt(i2) === '1')
                    ctx.fillStyle = v2 ? "#4caf50" : "#1a1a1a"
                    ctx.strokeStyle = "#555555"; ctx.lineWidth = 1
                    roundRect(ctx, gx2, gy2, gw2, gh2, 3 * viewScale); ctx.fill(); ctx.stroke()
                    ctx.fillStyle = v2 ? "#ffffff" : "#888888"
                    ctx.font = Math.max(8, Math.round(12 * viewScale)) + "px monospace"
                    ctx.textAlign = "center"; ctx.textBaseline = "middle"
                    ctx.fillText(v2 ? "1" : "0", gx2 + gw2/2, gy2 + gh2/2)
                }
            } else {
                var dispStr = circuit.formatValue(ovStr, base, bw)
                ctx.fillStyle = "#dddddd"
                ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                ctx.fillText(dispStr, sp.x + w/2, sp.y + h/2)
            }
        } else if (t === "led") {
            var lvStr = ""
            if (comp.inputPortValues && comp.inputPortValues.length > 0)
                lvStr = comp.inputPortValues[0] || ""
            if (base !== "bin") {
                var ldStr = circuit.formatValue(lvStr, base, bw)
                ctx.fillStyle = "#dddddd"
                ctx.font = Math.max(9, Math.round(13 * viewScale)) + "px monospace"
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                ctx.fillText(ldStr, sp.x + w/2, sp.y + h/2)
            } else {
                for (var i3 = 0; i3 < bw; ++i3) {
                    var cx2 = sp.x + (14 + i3 * 22) * viewScale
                    var cy2 = sp.y + h/2
                    var v3 = (i3 < lvStr.length && lvStr.charAt(i3) === '1')
                    ctx.fillStyle = v3 ? "#ffd700" : "#2a2a2a"
                    ctx.beginPath()
                    ctx.arc(cx2, cy2, 8 * viewScale, 0, Math.PI*2)
                    ctx.fill()
                    ctx.strokeStyle = v3 ? "#ffeb3b" : "#444444"
                    ctx.lineWidth = 1.5 * viewScale
                    ctx.stroke()
                }
            }
        } else {
            var label = ""
            if (t === "and") label = "AND"
            else if (t === "or") label = "OR"
            else if (t === "not") label = "NOT"
            else if (t === "nand") label = "NAND"
            else if (t === "nor") label = "NOR"
            else if (t === "xor") label = "XOR"
            else if (t === "xnor") label = "XNOR"
            else if (t === "splitter") label = "分线器"
            else if (t === "hub") label = "集线器"
            else if (t === "sub") label = comp.name || "SUB"
            if (label.length > 0) {
                ctx.fillStyle = "#e0e0e0"
                ctx.font = Math.max(8, Math.round(11 * viewScale)) + "px sans-serif"
                ctx.textAlign = "center"; ctx.textBaseline = "middle"
                ctx.fillText(label, sp.x + w/2, sp.y + h/2)
            }
        }

        var ins = inPortsById[comp.id] || []
        for (var k = 0; k < ins.length; k++) {
            var pIn = ins[k]
            var psp = worldToScreen(comp.x + pIn.x, comp.y + pIn.y)
            var portVal = false
            if (comp.inputPortValues && comp.inputPortValues.length > k) {
                var pvStr = comp.inputPortValues[k] || ""
                portVal = (pvStr.length > 0 && pvStr.charAt(0) === '1')
            }
            ctx.fillStyle = portVal ? "#4caf50" : "#999999"
            ctx.beginPath()
            ctx.arc(psp.x, psp.y, 6 * viewScale, 0, Math.PI*2)
            ctx.fill()
        }

        var outs = outPortsById[comp.id] || []
        for (var k2 = 0; k2 < outs.length; k2++) {
            var pOut = outs[k2]
            var psp2 = worldToScreen(comp.x + pOut.x, comp.y + pOut.y)
            var outVal = false
            if (comp.outputPorts && comp.outputPorts.length > k2) {
                var ovStr2 = comp.outputPorts[k2] || ""
                outVal = (ovStr2.length > 0 && ovStr2.charAt(0) === '1')
            }
            ctx.fillStyle = outVal ? "#4caf50" : "#999999"
            ctx.beginPath()
            ctx.arc(psp2.x, psp2.y, 6 * viewScale, 0, Math.PI*2)
            ctx.fill()
        }

        if (t === "sub") {
            var subIns = comp.subInputNames || []
            var subOuts = comp.subOutputNames || []
            ctx.font = Math.max(7, Math.round(9 * viewScale)) + "px sans-serif"
            ctx.fillStyle = "#a371f7"
            ctx.textAlign = "right"; ctx.textBaseline = "middle"
            for (var a = 0; a < ins.length && a < subIns.length; ++a) {
                var ppA = worldToScreen(comp.x + ins[a].x, comp.y + ins[a].y)
                ctx.fillText(subIns[a], ppA.x - 10 * viewScale, ppA.y)
            }
            ctx.textAlign = "left"
            for (var b = 0; b < outs.length && b < subOuts.length; ++b) {
                var ppB = worldToScreen(comp.x + outs[b].x, comp.y + outs[b].y)
                ctx.fillText(subOuts[b], ppB.x + 10 * viewScale, ppB.y)
            }
        }

        if (comp.name && comp.name.length > 0 && t !== "input" && t !== "output"
                && t !== "led" && t !== "sub") {
            ctx.fillStyle = "#cccccc"
            ctx.font = Math.max(8, Math.round(10 * viewScale)) + "px sans-serif"
            ctx.textAlign = "center"; ctx.textBaseline = "top"
            ctx.fillText(comp.name, sp.x + w/2, sp.y - 4*viewScale)
        }

        ctx.restore()
    }

    function roundRect(ctx, x, y, w, h, r) {
        if (r > w/2) r = w/2
        if (r > h/2) r = h/2
        ctx.beginPath()
        ctx.moveTo(x + r, y); ctx.lineTo(x + w - r, y)
        ctx.arcTo(x + w, y, x + w, y + r, r)
        ctx.lineTo(x + w, y + h - r)
        ctx.arcTo(x + w, y + h, x + w - r, y + h, r)
        ctx.lineTo(x + r, y + h)
        ctx.arcTo(x, y + h, x, y + h - r, r)
        ctx.lineTo(x, y + r)
        ctx.arcTo(x, y, x + r, y, r)
        ctx.closePath()
    }

    Dialog {
        id: subNameDialog
        anchors.centerIn: parent; modal: true
        title: "新建子电路"; standardButtons: Dialog.NoButton
        width: 320; padding: 20
        background: Rectangle { color: "#2a2a2a"; border.color: "#555555"; border.width: 1; radius: 10 }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: "输入子电路名称"; color: "#eeeeee"; font.pixelSize: 13 }
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 34
                color: "#1a1a1a"; border.color: "#555555"; border.width: 1; radius: 5
                TextField {
                    id: subNameInput
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                    color: "#dddddd"; font.pixelSize: 12
                    background: null; selectByMouse: true
                    onAccepted: {
                        if (text.trim().length === 0) return
                        circuit.addSubcircuit(text.trim())
                        subNameDialog.close()
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Rectangle { Layout.preferredWidth: 70; Layout.preferredHeight: 30; radius: 5
                    color: sn1.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#555555"; border.width: 1
                    Text { anchors.centerIn: parent; text: "取消"; color: "#dddddd"; font.pixelSize: 12 }
                    MouseArea { id: sn1; anchors.fill: parent; onClicked: subNameDialog.close() }
                }
                Rectangle { Layout.preferredWidth: 80; Layout.preferredHeight: 30; radius: 5
                    color: sn2.pressed ? "#2ea043" : "#238636"; border.color: "#2ea043"; border.width: 1
                    Text { anchors.centerIn: parent; text: "创建"; color: "#ffffff"; font.pixelSize: 12 }
                    MouseArea { id: sn2; anchors.fill: parent; onClicked: {
                        if (subNameInput.text.trim().length === 0) return
                        circuit.addSubcircuit(subNameInput.text.trim())
                        subNameDialog.close()
                    }}
                }
            }
        }
    }

    Dialog {
        id: closeConfirm
        anchors.centerIn: parent; modal: true
        title: "未保存"; standardButtons: Dialog.NoButton
        padding: 20; width: 340
        background: Rectangle { color: "#2a2a2a"; border.color: "#555555"; border.width: 1; radius: 10 }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: "电路已修改，是否保存？"; color: "#eeeeee"; font.pixelSize: 14
                Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Rectangle { Layout.preferredWidth: 80; Layout.preferredHeight: 32; radius: 5
                    color: cc1.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#555555"; border.width: 1
                    Text { anchors.centerIn: parent; text: "取消"; color: "#dddddd"; font.pixelSize: 12 }
                    MouseArea { id: cc1; anchors.fill: parent; onClicked: closeConfirm.close() }
                }
                Rectangle { Layout.preferredWidth: 100; Layout.preferredHeight: 32; radius: 5
                    color: cc2.pressed ? "#7a2020" : "#5a1a1a"; border.color: "#aa4040"; border.width: 1
                    Text { anchors.centerIn: parent; text: "不保存"; color: "#ffffff"; font.pixelSize: 12 }
                    MouseArea { id: cc2; anchors.fill: parent; onClicked: {
                        root.dirty = false; quitApp()
                    }}
                }
                Rectangle { Layout.preferredWidth: 80; Layout.preferredHeight: 32; radius: 5
                    color: cc3.pressed ? "#2ea043" : "#238636"; border.color: "#2ea043"; border.width: 1
                    Text { anchors.centerIn: parent; text: "保存"; color: "#ffffff"; font.pixelSize: 12 }
                    MouseArea { id: cc3; anchors.fill: parent; onClicked: {
                        closeConfirm.close()
                        saveBeforeExit = true
                        if (root.currentFilePath !== "") {
                            if (circuit.saveToFile(root.currentFilePath)) {
                                root.dirty = false
                                saveBeforeExit = false
                                quitApp()
                            } else {
                                saveBeforeExit = false
                                saveDialog.open()
                            }
                        } else {
                            saveDialog.open()
                        }
                    }}
                }
            }
        }
    }

    Dialog {
        id: exitConfirm
        anchors.centerIn: parent; modal: true
        title: "退出"; standardButtons: Dialog.NoButton
        padding: 20; width: 320
        background: Rectangle { color: "#2a2a2a"; border.color: "#555555"; border.width: 1; radius: 10 }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: "确定要退出 LogicSim 吗？"
                color: "#eeeeee"; font.pixelSize: 14
                Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Rectangle { Layout.preferredWidth: 80; Layout.preferredHeight: 32; radius: 5
                    color: ec1.pressed ? "#3a3a3a" : "#2d2d2d"
                    border.color: "#555555"; border.width: 1
                    Text { anchors.centerIn: parent; text: "取消"; color: "#dddddd"; font.pixelSize: 12 }
                    MouseArea { id: ec1; anchors.fill: parent; onClicked: exitConfirm.close() }
                }
                Rectangle { Layout.preferredWidth: 80; Layout.preferredHeight: 32; radius: 5
                    color: ec2.pressed ? "#2ea043" : "#238636"
                    border.color: "#2ea043"; border.width: 1
                    Text { anchors.centerIn: parent; text: "退出"; color: "#ffffff"; font.pixelSize: 12 }
                    MouseArea { id: ec2; anchors.fill: parent; onClicked: quitApp() }
                }
            }
        }
    }

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
                if (saveBeforeExit) { saveBeforeExit = false; quitApp() }
            } else {
                root.statusText = "保存失败：" + path
                saveBeforeExit = false
            }
        }
        onRejected: saveBeforeExit = false
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
                root.resetView()
                root.statusText = "已打开：" + path
            } else {
                root.statusText = "打开失败：" + path
            }
        }
    }

    Dialog {
        id: confirmDialog
        property string message: ""
        property var action: null
        anchors.centerIn: parent; modal: true
        standardButtons: Dialog.NoButton
        width: 320; padding: 20
        background: Rectangle { color: "#2a2a2a"; border.color: "#555555"; border.width: 1; radius: 10 }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: confirmDialog.title; color: "#eeeeee"; font.pixelSize: 14; font.bold: true
                Layout.fillWidth: true; wrapMode: Text.Wrap }
            Text { text: confirmDialog.message; color: "#aaaaaa"; font.pixelSize: 12
                Layout.fillWidth: true; wrapMode: Text.Wrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Rectangle { Layout.preferredWidth: 70; Layout.preferredHeight: 30; radius: 5
                    color: cd1.pressed ? "#3a3a3a" : "#2d2d2d"; border.color: "#555555"; border.width: 1
                    Text { anchors.centerIn: parent; text: "取消"; color: "#dddddd"; font.pixelSize: 12 }
                    MouseArea { id: cd1; anchors.fill: parent; onClicked: confirmDialog.close() }
                }
                Rectangle { Layout.preferredWidth: 80; Layout.preferredHeight: 30; radius: 5
                    color: cd2.pressed ? "#7a2020" : "#5a1a1a"; border.color: "#aa4040"; border.width: 1
                    Text { anchors.centerIn: parent; text: "确定"; color: "#ffffff"; font.pixelSize: 12 }
                    MouseArea { id: cd2; anchors.fill: parent; onClicked: {
                        if (confirmDialog.action) confirmDialog.action()
                        confirmDialog.close()
                    }}
                }
            }
        }
    }
}