import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Popup {
    id: statPopup
    property QtObject main: null

    anchors.centerIn: parent
    width: 340
    height: 440
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: Theme.bgDialog
        border.color: Theme.borderDialog
        border.width: 1
        radius: 8
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            height: 34
            color: Theme.bgDialogHeader
            Text {
                anchors.left: parent.left; anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: "电路统计（递归含子电路）"
                color: Theme.textWhite
                font.pixelSize: Theme.fsMedium
                font.bold: true
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
                x: 12; y: 6
                width: parent.width - 24
                spacing: 5

                Repeater {
                    model: statModel
                    delegate: RowLayout {
                        width: statCol.width
                        spacing: 10
                        Text {
                            text: model.k
                            color: Theme.textGray
                            font.pixelSize: Theme.fsSmall
                            Layout.fillWidth: true
                        }
                        Text {
                            text: model.v
                            color: Theme.text
                            font.pixelSize: Theme.fsSmall
                            font.bold: true
                        }
                    }
                }
            }
        }
    }

    ListModel { id: statModel }

    onOpened: refreshStats()

    function refreshStats() {
        var s = main.circuitRef.statistics()
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
