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

    property alias inputValueField: inputValueField
    property alias splitterSplitsInput: splitterProps.splitterSplitsInput
    property alias textContentField: textProps.textContentField

    // ★ 统一安全访问（null-safe）
    readonly property var selComp: main ? (main.selectedComp || null) : null
    readonly property string selCompId: main ? (main.selectedCompId || "") : ""
    readonly property string selCompType: selComp ? (selComp.type || "") : ""
    readonly property string selSubId: main ? (main.selectedSubId || "") : ""
    readonly property string selWireId: main ? (main.selectedWireId || "") : ""
    readonly property bool viewOnly: main ? main.circuitViewOnly : false
    readonly property string engineType: main ? (main.engineType || "") : ""
    readonly property int inputCount: selComp ? (selComp.inputCount || 2) : 2
    readonly property int bitWidth: selComp ? (selComp.bitWidth || 1) : 1
    readonly property string displayBase: selComp ? (selComp.displayBase || "bin") : "bin"
    readonly property string compName: selComp ? (selComp.name || "") : ""

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
            visible: (main && (main.mode === "select"
                               || propertyPanel.selComp !== null
                               || propertyPanel.selSubId !== ""
                               || propertyPanel.selWireId !== ""))
            x: 8
            text: main && main.mode === "select"
                  ? ("已选中 " + main.selectedIds.length + " 个元件")
                  : (propertyPanel.selWireId !== "" ? "连线属性" : "属性")
            color: Theme.textWeak
            font.pixelSize: Theme.fsSmall; font.bold: true
        }

        // 多选操作面板（有 wire 选中时隐藏）
        Column {
            visible: main && main.mode === "select" && propertyPanel.selWireId === ""
            x: 6; width: parent.width - 12; spacing: 4

            Rectangle {
                width: parent.width; height: 30; radius: Theme.smallRadius
                color: copyBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: main && main.selectedIds.length > 0 ? Theme.accent : Theme.borderStrong
                border.width: 1
                opacity: main && main.selectedIds.length > 0 ? 1 : 0.5
                Text {
                    anchors.centerIn: parent
                    text: "复制 (" + (main ? main.selectedIds.length : 0) + ")"
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                }
                MouseArea {
                    id: copyBtn; anchors.fill: parent
                    enabled: main && main.selectedIds.length > 0
                    onClicked: {
                        if (!main) return
                        main.clipboardData = main.circuitRef.copySelection(main.selectedIds)
                        main.statusText = "已复制 " + main.selectedIds.length + " 个元件"
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 30; radius: Theme.smallRadius
                visible: main && main.clipboardData !== null
                color: pasteBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderStrong; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "粘贴"
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                }
                MouseArea {
                    id: pasteBtn; anchors.fill: parent
                    onClicked: {
                        if (!main) return
                        var newIds = main.circuitRef.pasteSelection(main.clipboardData, 30, 30)
                        main.selectedIds = newIds
                        main.statusText = "已粘贴 " + newIds.length + " 个元件（已选中）"
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 30; radius: Theme.smallRadius
                visible: main && main.selectedIds.length > 0
                color: da.pressed ? "#6e2020" : Theme.bgButtonDanger
                border.color: Theme.borderDanger; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "删除选中"
                    color: Theme.textWhite; font.pixelSize: Theme.fsSmall
                }
                MouseArea {
                    id: da; anchors.fill: parent
                    onClicked: {
                        if (!main) return
                        main.circuitRef.removeComponents(main.selectedIds)
                        main.selectedIds = []
                    }
                }
            }
        }

        // 子电路属性
        SubcircuitPanel {
            id: subcircuitPanel
            main: propertyPanel.main
            visible: main && propertyPanel.selSubId !== "" && main.mode !== "select"
            x: 6
            width: parent.width - 12
        }

        // 连线属性
        Column {
            id: wirePropsColumn
            visible: main && propertyPanel.selWireId !== ""
            x: 6; width: parent.width - 12; spacing: 5

            Text {
                text: "连线"
                color: Theme.textWeak; font.pixelSize: Theme.fsSmall; font.bold: true
            }
            Text {
                text: "ID：" + propertyPanel.selWireId
                color: Theme.text; font.pixelSize: Theme.fsSmall
            }
            Text {
                text: "类型：" + (main ? (main.circuitRef.getWireType(propertyPanel.selWireId) || "bezier") : "bezier")
                color: Theme.text; font.pixelSize: Theme.fsSmall
            }

            Rectangle {
                width: parent.width; height: 30; radius: Theme.smallRadius
                color: delWireMa.pressed ? "#6e2020" : Theme.bgButtonDanger
                border.color: Theme.borderDanger; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "删除连线"
                    color: Theme.textWhite; font.pixelSize: Theme.fsSmall
                }
                MouseArea {
                    id: delWireMa; anchors.fill: parent
                    onClicked: {
                        if (!main) return
                        if (propertyPanel.selWireId === "") return
                        main.circuitRef.removeWire(propertyPanel.selWireId)
                        main.selectedWireId = ""
                    }
                }
            }
        }

        // 元件属性
        Column {
            id: compPropsColumn
            visible: main && propertyPanel.selComp !== null
                     && propertyPanel.selSubId === ""
                     && propertyPanel.selWireId === ""
            x: 6; width: parent.width - 12; spacing: 5

            Text {
                text: "ID：" + (propertyPanel.selComp ? (propertyPanel.selComp.id || "") : "")
                color: Theme.text; font.pixelSize: Theme.fsSmall
            }
            Text {
                text: "类型：" + propertyPanel.selCompType
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
                        text: propertyPanel.compName
                        selectByMouse: true
                        readOnly: propertyPanel.viewOnly
                        onEditingFinished: {
                            if (!main) return
                            if (propertyPanel.selCompId === "" || propertyPanel.viewOnly) return
                            main.circuitRef.renameComponent(propertyPanel.selCompId, text)
                        }
                    }
                }
            }

            Column {
                spacing: 4; width: parent.width
                visible: propertyPanel.selComp !== null && propertyPanel.selCompType !== "text"

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
                            property bool active: propertyPanel.selComp
                                ? ((propertyPanel.selComp.rotation || 0) === modelData.rot)
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
                                enabled: !propertyPanel.viewOnly
                                onClicked: {
                                    if (!main) return
                                    if (propertyPanel.selCompId === "") return
                                    main.circuitRef.setComponentProp(
                                        propertyPanel.selCompId, "rotation", modelData.rot)
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
                    enabled: !propertyPanel.viewOnly
                    value: propertyPanel.bitWidth
                    onValueChanged: {
                        if (!main) return
                        if (propertyPanel.viewOnly) return
                        if (propertyPanel.selComp && propertyPanel.selCompId !== "" &&
                            propertyPanel.selComp.bitWidth !== value)
                            main.circuitRef.setComponentProp(propertyPanel.selCompId, "bitWidth", value)
                    }
                }
            }

            Row {
                visible: propertyPanel.selComp !== null
                         && propertyPanel.isGateType(propertyPanel.selCompType)
                spacing: 4
                Text {
                    text: "输入"; color: Theme.textGray
                    font.pixelSize: Theme.fsSmall; width: 36
                    anchors.verticalCenter: parent.verticalCenter
                }
                SpinBox {
                    from: 2; to: 64; editable: true; width: 90
                    enabled: !propertyPanel.viewOnly
                    value: propertyPanel.inputCount
                    onValueChanged: {
                        if (!main) return
                        if (propertyPanel.viewOnly) return
                        if (propertyPanel.selComp && propertyPanel.selCompId !== "" &&
                            propertyPanel.selComp.inputCount !== value)
                            main.circuitRef.setComponentProp(propertyPanel.selCompId, "inputCount", value)
                    }
                }
            }

            Column {
                visible: propertyPanel.selComp !== null
                         && propertyPanel.isGateLike(propertyPanel.selCompType)
                         && propertyPanel.engineType === "event"
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
                                if (!main || propertyPanel.selComp === null) return ""
                                var v = main.circuitRef.getGateTypeDelay(propertyPanel.selCompType)
                                return String(v)
                            }
                            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,18}/ }
                            onEditingFinished: {
                                if (!main) return
                                if (propertyPanel.viewOnly) return
                                if (propertyPanel.selComp === null) return
                                var v = parseInt(text)
                                if (isNaN(v) || v < 1) return
                                main.circuitRef.setGateTypeDelay(propertyPanel.selCompType, v)
                                main.statusText = "已设置 " + propertyPanel.selCompType + " 延迟 = " + v
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
                                if (!main) return
                                if (propertyPanel.selComp === null) return
                                main.circuitRef.clearGateTypeDelays()
                                main.statusText = "已清除独立延迟"
                            }
                        }
                    }
                }
                Text {
                    text: "0 或空 = 使用全局（" + (main ? main.circuitRef.globalGateDelay : 1) + "）"
                    color: Theme.textFaint
                    font.pixelSize: Theme.fsTiny
                    width: parent.width
                    wrapMode: Text.Wrap
                }
            }

            ComboBox {
                id: baseCombo
                width: parent.width
                visible: propertyPanel.selComp !== null
                         && (propertyPanel.isInputLike(propertyPanel.selCompType)
                             || propertyPanel.selCompType === "output"
                             || propertyPanel.selCompType === "led")
                enabled: !propertyPanel.viewOnly
                model: {
                    var bw = propertyPanel.bitWidth
                    var arr = ["二进制", "无符号十进制", "有符号十进制", "十六进制"]
                    if (bw >= 32) arr.push("单精度浮点")
                    if (bw >= 64) arr.push("双精度浮点")
                    return arr
                }
                onActivated: {
                    if (!main) return
                    if (propertyPanel.selCompId === "" || propertyPanel.viewOnly) return
                    var bw = propertyPanel.bitWidth
                    var map = ["bin", "udec", "sdec", "hex"]
                    if (bw >= 32) map.push("f32")
                    if (bw >= 64) map.push("f64")
                    main.circuitRef.setInputBase(propertyPanel.selCompId, map[currentIndex] || "bin")
                }
                Connections {
                    target: main
                    function onSelectedCompChanged() {
                        if (!main) return
                        if (!propertyPanel.selComp) return
                        var b = propertyPanel.selComp.displayBase || "bin"
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
                visible: propertyPanel.selComp !== null
                         && propertyPanel.isInputLike(propertyPanel.selCompType)
                         && propertyPanel.displayBase !== "bin"
                spacing: 4; width: parent.width
                Text {
                    text: "值"; color: Theme.textGray
                    font.pixelSize: Theme.fsSmall
                }
                Rectangle {
                    width: parent.width; height: 26
                    color: Theme.bgInput
                    border.color: main && main.inputError.length > 0 ? "#aa4040" : Theme.borderStrong
                    border.width: 1; radius: Theme.smallRadius
                    TextField {
                        id: inputValueField
                        anchors.fill: parent
                        anchors.leftMargin: 5; anchors.rightMargin: 5
                        color: Theme.text
                        font.pixelSize: Theme.fsSmall; font.family: "monospace"
                        background: null; selectByMouse: true
                        readOnly: propertyPanel.viewOnly
                        onEditingFinished: {
                            if (!main) return
                            if (propertyPanel.selCompId === "" || propertyPanel.viewOnly) return
                            main.inputError = main.circuitRef.setInputFromString(
                                                propertyPanel.selCompId, text)
                        }
                    }
                }
                Text {
                    visible: main && main.inputError.length > 0
                    text: "⚠ " + (main ? main.inputError : "")
                    color: Theme.accentRed; font.pixelSize: Theme.fsTiny
                    width: parent.width; wrapMode: Text.Wrap
                }
            }

            Flow {
                width: parent.width; spacing: 3
                visible: propertyPanel.selComp !== null
                         && propertyPanel.isInputLike(propertyPanel.selCompType)
                         && propertyPanel.displayBase === "bin"
                Repeater {
                    model: {
                        var bw = propertyPanel.bitWidth
                        var arr = []
                        for (var i = 0; i < bw; ++i) arr.push(i)
                        return arr
                    }
                    delegate: Rectangle {
                        width: 22; height: 22; radius: 3
                        color: {
                            if (!propertyPanel.selComp) return Theme.bgButton
                            var bits = propertyPanel.selComp.inputBits || []
                            return (bits[index] || false) ? Theme.accentGreen : Theme.bgButton
                        }
                        border.color: Theme.borderDialog; border.width: 1
                        opacity: propertyPanel.viewOnly ? 0.7 : 1
                        Text {
                            anchors.centerIn: parent
                            text: {
                                if (!propertyPanel.selComp) return "0"
                                var bits = propertyPanel.selComp.inputBits || []
                                return (bits[index] || false) ? "1" : "0"
                            }
                            color: Theme.textWhite; font.pixelSize: Theme.fsTiny
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: !propertyPanel.viewOnly
                            onClicked: {
                                if (!main) return
                                if (propertyPanel.selCompId === "") return
                                var bits = propertyPanel.selComp.inputBits || []
                                main.circuitRef.setBitValue(
                                    propertyPanel.selCompId, index, !(bits[index] || false))
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
                visible: propertyPanel.selComp !== null
                         && (propertyPanel.selCompType === "tgate"
                             || propertyPanel.selCompType === "ntran"
                             || propertyPanel.selCompType === "ptran")
                spacing: 4; width: parent.width
                Text {
                    text: propertyPanel.selCompType === "tgate"
                          ? "端口：D / EN → Y\nEN=1 时 Y=D；EN=0 时输出高阻 Z"
                          : (propertyPanel.selCompType === "ntran"
                             ? "端口：G(左) / D(右下) / S(右上)\nG=1 时导通 S=D；G=0 时输出高阻 Z"
                             : "端口：G(左) / D(右上) / S(右下)\nG=0 时导通 S=D；G=1 时输出高阻 Z")
                    color: Theme.textFaint
                    font.pixelSize: Theme.fsTiny
                    width: parent.width; wrapMode: Text.Wrap
                }
            }

            Rectangle {
                width: parent.width; height: 30; radius: Theme.smallRadius
                visible: !propertyPanel.viewOnly
                color: delMa.pressed ? "#6e2020" : Theme.bgButtonDanger
                border.color: Theme.borderDanger; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "删除"
                    color: Theme.textWhite; font.pixelSize: Theme.fsSmall
                }
                MouseArea {
                    id: delMa; anchors.fill: parent
                    onClicked: {
                        if (!main) return
                        main.confirmDelete(propertyPanel.selCompId)
                    }
                }
            }
        }

        Rectangle {
            visible: main && main.errorList.length > 0
            x: 8; width: parent.width - 16; height: 1
            color: Theme.border
        }
        Text {
            visible: main && main.errorList.length > 0
            x: 8
            text: "错误 (" + (main ? main.errorList.length : 0) + ")"
            color: Theme.accentRed
            font.pixelSize: Theme.fsSmall; font.bold: true
        }
        Repeater {
            model: main ? main.errorList : []
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