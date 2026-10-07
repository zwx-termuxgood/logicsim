import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Rectangle {
    id: leftPanel
    property QtObject main: null

    property alias inputValueField: inputValueField
    property alias splitterSplitsInput: splitterSplitsInput
    property alias textContentField: textContentField

    anchors.left: parent.left
    anchors.bottom: parent.bottom
    width: Theme.leftPanelW
    color: Theme.bgPanel
    border.color: Theme.border
    border.width: 1

    function isGateType(t) {
        return t === "and" || t === "or" || t === "nand" ||
               t === "nor" || t === "xor" || t === "xnor"
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
        { name: "传输门 / 晶体管", items: [
            { type: "tgate", name: "传输门" },
            { type: "ntran", name: "NMOS 管" },
            { type: "ptran", name: "PMOS 管" }
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ============ 元件库 ============
        Flickable {
            id: paletteFlick
            Layout.fillWidth: true
            Layout.preferredHeight: main.mode !== "control" && !main.circuitViewOnly
                                  ? Math.round(main.windowHeight * 0.5) : 0
            visible: main.mode !== "control" && !main.circuitViewOnly
            clip: true
            contentWidth: width
            contentHeight: paletteColumn.height + 12
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: paletteColumn
                width: paletteFlick.width
                spacing: 3
                Item { width: 1; height: 4 }

                Repeater {
                    model: leftPanel.elementGroups
                    delegate: Column {
                        width: paletteColumn.width
                        spacing: 2
                        Rectangle {
                            width: parent.width; height: 22; color: Theme.bgDialog
                            Text {
                                anchors.left: parent.left; anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name
                                color: Theme.textWeak
                                font.pixelSize: Theme.fsSmall; font.bold: true
                            }
                        }
                        Repeater {
                            model: modelData.items
                            delegate: Rectangle {
                                width: parent.width - 10; x: 5; height: 28
                                radius: Theme.smallRadius
                                color: (main.placingType === modelData.type && main.placingSubId === "")
                                       ? Theme.bgButtonHover
                                       : (itemMa.pressed ? Theme.bgButtonHover : Theme.bgButton)
                                border.color: (main.placingType === modelData.type && main.placingSubId === "")
                                              ? Theme.borderBtnActive : Theme.borderWeak
                                border.width: 1
                                Text {
                                    anchors.left: parent.left; anchors.leftMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.name
                                    color: Theme.text; font.pixelSize: Theme.fsNormal
                                }
                                MouseArea {
                                    id: itemMa; anchors.fill: parent
                                    onClicked: {
                                        if (main.placingType === modelData.type && main.placingSubId === "") {
                                            main.placingType = ""
                                        } else {
                                            main.placingType = modelData.type
                                            main.placingSubId = ""
                                            main.statusText = "已选择 " + modelData.name + "，点画布放置"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width; height: 22; color: Theme.bgDialog
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: "子电路 (" + main.subList.length + ")"
                        color: Theme.accentPurple
                        font.pixelSize: Theme.fsSmall; font.bold: true
                    }
                }
                Rectangle {
                    width: parent.width - 10; x: 5; height: 28; radius: Theme.smallRadius
                    color: newSubMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                    border.color: Theme.borderSub; border.width: 1
                    Text {
                        anchors.centerIn: parent; text: "+ 新建子电路"
                        color: Theme.accentPurple; font.pixelSize: Theme.fsSmall
                    }
                    MouseArea { id: newSubMa; anchors.fill: parent
                        onClicked: main.subNameDialogRef.open() }
                }
                Repeater {
                    model: main.subList
                    delegate: Rectangle {
                        width: parent.width - 10; x: 5; height: 32
                        radius: Theme.smallRadius
                        color: (main.placingType === "sub" && main.placingSubId === modelData.id)
                               ? Theme.bgButtonHover
                               : (subItemMa.pressed ? Theme.bgButtonHover : Theme.bgButton)
                        border.color: (main.placingType === "sub" && main.placingSubId === modelData.id)
                                      ? Theme.accentPurple : Theme.borderWeak
                        border.width: 1
                        Text {
                            anchors.left: parent.left; anchors.leftMargin: 10
                            anchors.right: parent.right; anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.name + " [" + modelData.inCount + "/" + modelData.outCount + "]"
                            color: Theme.text; font.pixelSize: Theme.fsSmall
                            elide: Text.ElideRight
                        }
                        MouseArea {
                            id: subItemMa; anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.LeftButton) {
                                    main.placingType = "sub"
                                    main.placingSubId = modelData.id
                                    main.statusText = "已选择 " + modelData.name + "，点画布放置"
                                } else if (mouse.button === Qt.RightButton) {
                                    subCtxMenu.subId = modelData.id
                                    subCtxMenu.subName = modelData.name
                                    subCtxMenu.popup()
                                }
                            }
                        }
                    }
                }
                Item { width: 1; height: 8 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        // ============ 属性面板 ============
        Flickable {
            id: propsFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: propsColumn.height + 12
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: propsColumn
                width: propsFlick.width
                spacing: 5
                Item { width: 1; height: 6 }

                Text {
                    x: 8
                    text: main.mode === "select"
                          ? ("已选中 " + main.selectedIds.length + " 个元件")
                          : "属性"
                    color: Theme.textWeak
                    font.pixelSize: Theme.fsSmall; font.bold: true
                }

                Column {
                    visible: main.mode === "select"
                    x: 6; width: parent.width - 12; spacing: 4

                    Rectangle {
                        width: parent.width; height: 30; radius: Theme.smallRadius
                        color: copyBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
                        border.color: main.selectedIds.length > 0 ? Theme.accent : Theme.borderStrong
                        border.width: 1
                        opacity: main.selectedIds.length > 0 ? 1 : 0.5
                        Text {
                            anchors.centerIn: parent
                            text: "复制 (" + main.selectedIds.length + ")"
                            color: Theme.text; font.pixelSize: Theme.fsSmall
                        }
                        MouseArea {
                            id: copyBtn; anchors.fill: parent
                            enabled: main.selectedIds.length > 0
                            onClicked: {
                                main.clipboardData = main.circuitRef.copySelection(main.selectedIds)
                                main.statusText = "已复制 " + main.selectedIds.length + " 个元件"
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width; height: 30; radius: Theme.smallRadius
                        visible: main.clipboardData !== null
                        color: pasteBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
                        border.color: Theme.borderStrong; border.width: 1
                        Text {
                            anchors.centerIn: parent; text: "粘贴"
                            color: Theme.text; font.pixelSize: Theme.fsSmall
                        }
                        MouseArea {
                            id: pasteBtn; anchors.fill: parent
                            onClicked: {
                                var newIds = main.circuitRef.pasteSelection(main.clipboardData, 30, 30)
                                main.selectedIds = newIds
                                main.statusText = "已粘贴 " + newIds.length + " 个元件（已选中）"
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width; height: 30; radius: Theme.smallRadius
                        visible: main.selectedIds.length > 0
                        color: da.pressed ? "#6e2020" : Theme.bgButtonDanger
                        border.color: Theme.borderDanger; border.width: 1
                        Text {
                            anchors.centerIn: parent; text: "删除选中"
                            color: Theme.textWhite; font.pixelSize: Theme.fsSmall
                        }
                        MouseArea {
                            id: da; anchors.fill: parent
                            onClicked: {
                                main.circuitRef.removeComponents(main.selectedIds)
                                main.selectedIds = []
                            }
                        }
                    }
                }

                Text {
                    visible: main.selectedComp === null && main.selectedIds.length === 0
                    x: 8; width: parent.width - 16
                    text: main.mode === "select"
                          ? "拖拽画布空白处框选，或点元件选中"
                          : "未选中元件"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fsSmall
                    wrapMode: Text.Wrap
                }

                Column {
                    visible: main.selectedComp !== null && main.mode !== "select"
                    x: 6; width: parent.width - 12; spacing: 5

                    Text {
                        text: "ID：" + (main.selectedComp ? main.selectedComp.id : "")
                        color: Theme.text; font.pixelSize: Theme.fsSmall
                    }
                    Text {
                        text: "类型：" + (main.selectedComp ? main.selectedComp.type : "")
                        color: Theme.text; font.pixelSize: Theme.fsSmall
                    }

                    Row {
                        spacing: 4
                        Text {
                            text: "名称"; color: Theme.textGray
                            font.pixelSize: Theme.fsSmall; width: 36
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Rectangle {
                            width: parent.parent.width - 42; height: 26
                            color: Theme.bgInput
                            border.color: Theme.borderStrong; border.width: 1
                            radius: Theme.smallRadius
                            TextField {
                                anchors.fill: parent
                                anchors.leftMargin: 5; anchors.rightMargin: 5
                                color: Theme.text; font.pixelSize: Theme.fsSmall
                                background: null
                                text: main.selectedComp ? (main.selectedComp.name || "") : ""
                                selectByMouse: true
                                readOnly: main.circuitViewOnly
                                onEditingFinished: {
                                    if (main.selectedCompId === "" || main.circuitViewOnly) return
                                    main.circuitRef.renameComponent(main.selectedCompId, text)
                                }
                            }
                        }
                    }

                    Row {
                        spacing: 4
                        Text {
                            text: "位宽"; color: Theme.textGray
                            font.pixelSize: Theme.fsSmall; width: 36
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        SpinBox {
                            from: 1; to: 64; editable: true; width: 90
                            enabled: !main.circuitViewOnly
                            value: main.selectedComp ? (main.selectedComp.bitWidth || 1) : 1
                            onValueChanged: {
                                if (main.circuitViewOnly) return
                                if (main.selectedComp && main.selectedCompId !== "" &&
                                    main.selectedComp.bitWidth !== value)
                                    main.circuitRef.setComponentProp(main.selectedCompId, "bitWidth", value)
                            }
                        }
                    }

                    Row {
                        visible: main.selectedComp && leftPanel.isGateType(main.selectedComp.type)
                        spacing: 4
                        Text {
                            text: "输入"; color: Theme.textGray
                            font.pixelSize: Theme.fsSmall; width: 36
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        SpinBox {
                            from: 2; to: 64; editable: true; width: 90
                            enabled: !main.circuitViewOnly
                            value: main.selectedComp ? (main.selectedComp.inputCount || 2) : 2
                            onValueChanged: {
                                if (main.circuitViewOnly) return
                                if (main.selectedComp && main.selectedCompId !== "" &&
                                    main.selectedComp.inputCount !== value)
                                    main.circuitRef.setComponentProp(main.selectedCompId, "inputCount", value)
                            }
                        }
                    }

                    ComboBox {
                        id: baseCombo
                        width: parent.width
                        visible: main.selectedComp && (main.selectedComp.type === "input" ||
                                                       main.selectedComp.type === "output" ||
                                                       main.selectedComp.type === "led")
                        enabled: !main.circuitViewOnly
                        model: {
                            var bw = main.selectedComp ? (main.selectedComp.bitWidth || 1) : 1
                            var arr = ["二进制", "无符号十进制", "有符号十进制", "十六进制"]
                            if (bw >= 32) arr.push("单精度浮点")
                            if (bw >= 64) arr.push("双精度浮点")
                            return arr
                        }
                        onActivated: {
                            if (main.selectedCompId === "" || main.circuitViewOnly) return
                            var bw = main.selectedComp.bitWidth || 1
                            var map = ["bin", "udec", "sdec", "hex"]
                            if (bw >= 32) map.push("f32")
                            if (bw >= 64) map.push("f64")
                            main.circuitRef.setInputBase(main.selectedCompId, map[currentIndex] || "bin")
                        }
                        Connections {
                            target: main
                            function onSelectedCompChanged() {
                                if (!main.selectedComp) return
                                var b = main.selectedComp.displayBase || "bin"
                                var map = { "bin":0, "udec":1, "sdec":2, "hex":3, "f32":4, "f64":5 }
                                baseCombo.currentIndex = map[b] !== undefined ? map[b] : 0
                            }
                        }
                    }

                    Column {
                        visible: main.selectedComp && main.selectedComp.type === "input" &&
                                 (main.selectedComp.displayBase || "bin") !== "bin"
                        spacing: 4; width: parent.width
                        Text {
                            text: "输入值"; color: Theme.textGray
                            font.pixelSize: Theme.fsSmall
                        }
                        Rectangle {
                            width: parent.width; height: 26
                            color: Theme.bgInput
                            border.color: main.inputError.length > 0 ? "#aa4040" : Theme.borderStrong
                            border.width: 1; radius: Theme.smallRadius
                            TextField {
                                id: inputValueField
                                anchors.fill: parent
                                anchors.leftMargin: 5; anchors.rightMargin: 5
                                color: Theme.text
                                font.pixelSize: Theme.fsSmall; font.family: "monospace"
                                background: null; selectByMouse: true
                                readOnly: main.circuitViewOnly
                                onEditingFinished: {
                                    if (main.selectedCompId === "" || main.circuitViewOnly) return
                                    main.inputError = main.circuitRef.setInputFromString(
                                                        main.selectedCompId, text)
                                }
                            }
                        }
                        Text {
                            visible: main.inputError.length > 0
                            text: "⚠ " + main.inputError
                            color: Theme.accentRed; font.pixelSize: Theme.fsTiny
                            width: parent.width; wrapMode: Text.Wrap
                        }
                    }

                    Flow {
                        width: parent.width; spacing: 3
                        visible: main.selectedComp && main.selectedComp.type === "input" &&
                                 (main.selectedComp.displayBase || "bin") === "bin"
                        Repeater {
                            model: {
                                if (!main.selectedComp) return []
                                var bw = main.selectedComp.bitWidth || 1
                                var arr = []
                                for (var i = 0; i < bw; ++i) arr.push(i)
                                return arr
                            }
                            delegate: Rectangle {
                                width: 22; height: 22; radius: 3
                                color: {
                                    if (!main.selectedComp) return Theme.bgButton
                                    var bits = main.selectedComp.inputBits || []
                                    return (bits[index] || false) ? Theme.accentGreen : Theme.bgButton
                                }
                                border.color: Theme.borderDialog; border.width: 1
                                opacity: main.circuitViewOnly ? 0.7 : 1
                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (!main.selectedComp) return "0"
                                        var bits = main.selectedComp.inputBits || []
                                        return (bits[index] || false) ? "1" : "0"
                                    }
                                    color: Theme.textWhite; font.pixelSize: Theme.fsTiny
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: !main.circuitViewOnly
                                    onClicked: {
                                        if (main.selectedCompId === "") return
                                        var bits = main.selectedComp.inputBits || []
                                        main.circuitRef.setBitValue(
                                            main.selectedCompId, index, !(bits[index] || false))
                                    }
                                }
                            }
                        }
                    }

                    Column {
                        visible: main.selectedComp &&
                                 (main.selectedComp.type === "splitter" ||
                                  main.selectedComp.type === "hub")
                        spacing: 4; width: parent.width
                        Text {
                            text: "分段（逗号分隔，和 = 位宽）"
                            color: Theme.textGray; font.pixelSize: Theme.fsSmall
                        }
                        Rectangle {
                            width: parent.width; height: 26
                            color: Theme.bgInput
                            border.color: Theme.borderStrong
                            border.width: 1; radius: Theme.smallRadius
                            TextField {
                                id: splitterSplitsInput
                                anchors.fill: parent
                                anchors.leftMargin: 5; anchors.rightMargin: 5
                                color: Theme.text
                                font.pixelSize: Theme.fsSmall; font.family: "monospace"
                                background: null; selectByMouse: true
                                readOnly: main.circuitViewOnly
                                onEditingFinished: {
                                    if (main.selectedCompId === "" || main.circuitViewOnly) return
                                    main.circuitRef.setSplitterSplits(main.selectedCompId, text)
                                }
                            }
                        }
                        Text {
                            text: main.selectedComp && main.selectedComp.type === "splitter"
                                  ? "分线器：1 输入 → N 输出"
                                  : "集线器：N 输入 → 1 输出"
                            color: Theme.textFaint
                            font.pixelSize: Theme.fsTiny
                            width: parent.width; wrapMode: Text.Wrap
                        }
                    }

                    Column {
                        visible: main.selectedComp && main.selectedComp.type === "text"
                        spacing: 4; width: parent.width
                        Text {
                            text: "文字内容"; color: Theme.textGray
                            font.pixelSize: Theme.fsSmall
                        }
                        Rectangle {
                            width: parent.width; height: 26
                            color: Theme.bgInput
                            border.color: Theme.borderStrong
                            border.width: 1; radius: Theme.smallRadius
                            TextField {
                                id: textContentField
                                anchors.fill: parent
                                anchors.leftMargin: 5; anchors.rightMargin: 5
                                color: Theme.text
                                font.pixelSize: Theme.fsSmall
                                background: null; selectByMouse: true
                                readOnly: main.circuitViewOnly
                                onEditingFinished: {
                                    if (main.selectedCompId === "" || main.circuitViewOnly) return
                                    main.circuitRef.setTextContent(main.selectedCompId, text)
                                }
                            }
                        }
                    }

                    // ---- 传输门 / 晶体管说明 ----
                    Column {
                        visible: main.selectedComp &&
                                 (main.selectedComp.type === "tgate" ||
                                  main.selectedComp.type === "ntran" ||
                                  main.selectedComp.type === "ptran")
                        spacing: 4; width: parent.width
                        Text {
                            text: main.selectedComp && main.selectedComp.type === "tgate"
                                  ? "端口：D / EN → Y\nEN=1 时 Y=D；EN=0 时输出高阻 Z"
                                  : (main.selectedComp && main.selectedComp.type === "ntran"
                                     ? "端口：G(左) / D(右上) / S(右下)\nG=1 时导通 S=D；G=0 时输出高阻 Z"
                                     : "端口：G(左) / S(右上) / D(右下)\nG=0 时导通 S=D；G=1 时输出高阻 Z")
                            color: Theme.textFaint
                            font.pixelSize: Theme.fsTiny
                            width: parent.width; wrapMode: Text.Wrap
                        }
                    }

                    Rectangle {
                        width: parent.width; height: 30; radius: Theme.smallRadius
                        visible: !main.circuitViewOnly
                        color: delMa.pressed ? "#6e2020" : Theme.bgButtonDanger
                        border.color: Theme.borderDanger; border.width: 1
                        Text {
                            anchors.centerIn: parent; text: "删除"
                            color: Theme.textWhite; font.pixelSize: Theme.fsSmall
                        }
                        MouseArea {
                            id: delMa; anchors.fill: parent
                            onClicked: main.confirmDelete(main.selectedCompId)
                        }
                    }
                }

                Rectangle {
                    visible: main.errorList.length > 0
                    x: 8; width: parent.width - 16; height: 1
                    color: Theme.border
                }
                Text {
                    visible: main.errorList.length > 0
                    x: 8
                    text: "错误 (" + main.errorList.length + ")"
                    color: Theme.accentRed
                    font.pixelSize: Theme.fsSmall; font.bold: true
                }
                Repeater {
                    model: main.errorList
                    delegate: Text {
                        x: 8; width: propsColumn.width - 16
                        text: "⚠ " + modelData.msg
                        color: Theme.accentRed
                        font.pixelSize: Theme.fsTiny
                        wrapMode: Text.Wrap
                    }
                }

                Item { width: 1; height: 20 }
            }
        }
    }

    // ============ 子电路右键菜单 ============
    Menu {
        id: subCtxMenu
        property string subId: ""
        property string subName: ""

        MenuItem {
            text: "重命名"
            onTriggered: {
                renameDialog.subId = subCtxMenu.subId
                renameInput.text = subCtxMenu.subName
                renameDialog.open()
            }
        }
        MenuItem {
            text: "设为主电路"
            onTriggered: main.circuitRef.setSubcircuitAsRoot(subCtxMenu.subId)
        }
        MenuSeparator { }
        MenuItem {
            text: "删除"
            onTriggered: {
                main.confirmDialogRef.title = "删除子电路"
                main.confirmDialogRef.message = "确定删除该子电路及其所有实例？"
                main.confirmDialogRef.action = function() {
                    main.circuitRef.deleteSubcircuit(subCtxMenu.subId)
                }
                main.confirmDialogRef.open()
            }
        }
    }

    Dialog {
        id: renameDialog
        property string subId: ""

        anchors.centerIn: parent
        modal: true
        title: "重命名子电路"
        standardButtons: Dialog.NoButton
        width: 300
        padding: 20

        background: Rectangle {
            color: Theme.bgDialog
            border.color: Theme.borderDialog
            border.width: 1
            radius: Theme.dialogRadius
        }

        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "输入新名称"
                color: Theme.textStrong
                font.pixelSize: Theme.fsMedium
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                color: Theme.bgInput
                border.color: Theme.borderDialog
                border.width: 1
                radius: Theme.smallRadius
                TextField {
                    id: renameInput
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    color: Theme.text
                    font.pixelSize: Theme.fsNormal
                    background: null
                    selectByMouse: true
                    onAccepted: renameDialog.doRename()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Rectangle {
                    Layout.preferredWidth: 70; Layout.preferredHeight: 30
                    radius: Theme.buttonRadius
                    color: cc1r.pressed ? Theme.bgButtonHover : Theme.bgButton
                    border.color: Theme.borderDialog; border.width: 1
                    Text { anchors.centerIn: parent; text: "取消"
                        color: Theme.text; font.pixelSize: Theme.fsNormal }
                    MouseArea { id: cc1r; anchors.fill: parent
                        onClicked: renameDialog.close() }
                }
                Rectangle {
                    Layout.preferredWidth: 80; Layout.preferredHeight: 30
                    radius: Theme.buttonRadius
                    color: cc2r.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                    border.color: Theme.borderSuccess; border.width: 1
                    Text { anchors.centerIn: parent; text: "确定"
                        color: Theme.textWhite; font.pixelSize: Theme.fsNormal }
                    MouseArea { id: cc2r; anchors.fill: parent
                        onClicked: renameDialog.doRename() }
                }
            }
        }

        function doRename() {
            var name = renameInput.text.trim()
            if (name.length === 0) return
            main.circuitRef.setSubcircuitName(renameDialog.subId, name)
            renameDialog.close()
        }
    }
}