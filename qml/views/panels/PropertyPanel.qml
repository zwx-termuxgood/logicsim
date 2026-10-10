import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim
import "props"

Flickable {
    id: propertyPanel
    property QtObject main: null

    clip: true
    contentWidth: width
    contentHeight: propsColumn.height + 12
    boundsBehavior: Flickable.StopAtBounds

    // 向外转发 3 个 alias（Main.qml 通过 leftPanel.xxx 访问）
    property alias inputValueField: inputValueField
    property alias splitterSplitsInput: splitterProps.splitterSplitsInput
    property alias textContentField: textProps.textContentField

    function isGateType(t) {
        return t === "and" || t === "or" || t === "nand" ||
               t === "nor" || t === "xor" || t === "xnor"
    }
    function isGateLike(t) {
        return t === "and" || t === "or" || t === "nand" ||
               t === "nor" || t === "xor" || t === "xnor" ||
               t === "not" || t === "tgate" || t === "ntran" ||
               t === "ptran" || t === "splitter" || t === "hub"
    }
    function isInputLike(t) {
        return t === "input" || t === "const"
    }

    Column {
        id: propsColumn
        width: propertyPanel.width
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

        // ============ 多选操作面板 ============
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

        // ============ 子电路属性 ============
        SubcircuitPanel {
            id: subcircuitPanel
            main: propertyPanel.main
            visible: main.selectedSubId !== "" && main.mode !== "select"
            x: 6
            width: parent.width - 12
        }

        // ============ 未选中提示 ============
        Text {
            visible: main.selectedComp === null
                     && main.selectedIds.length === 0
                     && main.selectedSubId === ""
            x: 8; width: parent.width - 16
            text: main.mode === "select"
                  ? "拖拽画布空白处框选，或点元件选中"
                  : "未选中元件"
            color: Theme.textMuted
            font.pixelSize: Theme.fsSmall
            wrapMode: Text.Wrap
        }

        // ============ 元件属性 ============
        Column {
            id: compPropsColumn
            visible: main.selectedComp !== null
                     && main.selectedSubId === ""
                     && main.mode !== "select"
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
                    width: compPropsColumn.width - 42; height: 26
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

            Column {
                spacing: 4; width: parent.width
                visible: main.selectedComp !== null && main.selectedComp.type !== "text"

                Text {
                    text: "方向"
                    color: Theme.textGray; font.pixelSize: Theme.fsSmall
                }
                Row {
                    spacing: 6
                    Repeater {
                        model: [
                            { deg: 0,   label: "→", rot: 0 },
                            { deg: 90,  label: "↓", rot: 1 },
                            { deg: 180, label: "←", rot: 2 },
                            { deg: 270, label: "↑", rot: 3 }
                        ]
                        delegate: Rectangle {
                            width: 30; height: 30; radius: Theme.smallRadius
                            property bool active: main.selectedComp
                                ? ((main.selectedComp.rotation || 0) === modelData.rot)
                                : (modelData.rot === 0)
                            color: active ? Theme.bgButtonActive
                                          : (rotMa.pressed ? Theme.bgButtonHover : Theme.bgButton)
                            border.color: active ? Theme.borderBtnActive : Theme.borderStrong
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: active ? Theme.textWhite : Theme.text
                                font.pixelSize: 16
                            }
                            MouseArea {
                                id: rotMa
                                anchors.fill: parent
                                enabled: !main.circuitViewOnly
                                onClicked: {
                                    if (main.selectedCompId === "") return
                                    main.circuitRef.setComponentProp(
                                        main.selectedCompId, "rotation", modelData.rot)
                                }
                            }
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
                visible: main.selectedComp && propertyPanel.isGateType(main.selectedComp.type)
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

            Column {
                visible: main.selectedComp
                         && propertyPanel.isGateLike(main.selectedComp.type)
                         && main.engineType === "event"
                spacing: 4
                width: parent.width

                Text {
                    text: "传播延迟（该类型门）"
                    color: Theme.textGray
                    font.pixelSize: Theme.fsSmall
                }

                Row {
                    spacing: 6
                    Rectangle {
                        width: 90; height: 26
                        color: Theme.bgInput
                        border.color: Theme.borderStrong
                        border.width: 1; radius: Theme.smallRadius
                        TextField {
                            anchors.fill: parent
                            anchors.leftMargin: 5; anchors.rightMargin: 5
                            color: Theme.text
                            font.pixelSize: Theme.fsSmall
                            background: null
                            selectByMouse: true
                            text: {
                                if (!main.selectedComp) return ""
                                var v = main.circuitRef.getGateTypeDelay(main.selectedComp.type)
                                return String(v)
                            }
                            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,18}/ }
                            onEditingFinished: {
                                if (main.circuitViewOnly) return
                                if (!main.selectedComp) return
                                var v = parseInt(text)
                                if (isNaN(v) || v < 1) return
                                main.circuitRef.setGateTypeDelay(main.selectedComp.type, v)
                                main.statusText = "已设置 " + main.selectedComp.type + " 延迟 = " + v
                            }
                        }
                    }
                    Rectangle {
                        width: 80; height: 26; radius: Theme.smallRadius
                        color: gateResetMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                        border.color: Theme.borderStrong; border.width: 1
                        Text {
                            anchors.centerIn: parent; text: "使用全局"
                            color: Theme.text; font.pixelSize: Theme.fsTiny
                        }
                        MouseArea {
                            id: gateResetMa
                            anchors.fill: parent
                            onClicked: {
                                if (!main.selectedComp) return
                                main.circuitRef.clearGateTypeDelays()
                                main.statusText = "已清除独立延迟"
                            }
                        }
                    }
                }

                Text {
                    text: "0 或空 = 使用全局（" + main.circuitRef.globalGateDelay + "）"
                    color: Theme.textFaint
                    font.pixelSize: Theme.fsTiny
                    width: parent.width
                    wrapMode: Text.Wrap
                }
            }

            ComboBox {
                id: baseCombo
                width: parent.width
                visible: (main.selectedComp && propertyPanel.isInputLike(main.selectedComp.type))
                         || (main.selectedComp && main.selectedComp.type === "output")
                         || (main.selectedComp && main.selectedComp.type === "led")
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

            LedProps {
                main: propertyPanel.main
                width: parent.width
            }

            Column {
                visible: main.selectedComp && propertyPanel.isInputLike(main.selectedComp.type) &&
                         (main.selectedComp.displayBase || "bin") !== "bin"
                spacing: 4; width: parent.width
                Text {
                    text: "值"; color: Theme.textGray
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
                visible: main.selectedComp && propertyPanel.isInputLike(main.selectedComp.type) &&
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

            SplitterProps {
                id: splitterProps
                main: propertyPanel.main
                width: parent.width
            }

            TextProps {
                id: textProps
                main: propertyPanel.main
                width: parent.width
            }

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
                             ? "端口：G(左) / D(右下) / S(右上)\nG=1 时导通 S=D；G=0 时输出高阻 Z"
                             : "端口：G(左) / D(右上) / S(右下)\nG=0 时导通 S=D；G=1 时输出高阻 Z")
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
