import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Rectangle {
    id: settingsPage
    property QtObject main: null

    color: Theme.bg
    z: 2000

    property int engineIndex: 0
    property string gateDelayText: "1"
    property string wireDelayText: "1"
    property int speedExp: 3
    property bool autoSpeed: true
    property string errorText: ""

    function syncFromCircuit() {
        engineIndex = (main.circuitRef.engineType === "event") ? 0 : 1
        gateDelayText = String(main.circuitRef.globalGateDelay)
        wireDelayText = String(main.circuitRef.wireDelay)
        autoSpeed = main.circuitRef.autoSpeed
        speedExp = main.circuitRef.simulationSpeedExp
        errorText = ""
    }

    Component.onCompleted: syncFromCircuit()
    onVisibleChanged: if (visible) syncFromCircuit()

    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 50
        color: Theme.bgPanel
        border.color: Theme.border
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12; anchors.rightMargin: 12
            spacing: 8

            Text {
                text: "⚙ 设置"
                color: Theme.textStrong
                font.pixelSize: Theme.fsLarge
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                Layout.preferredWidth: 90; Layout.preferredHeight: 32
                radius: Theme.buttonRadius
                color: backMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderStrong; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "← 返回"
                    color: Theme.text; font.pixelSize: Theme.fsNormal
                }
                MouseArea {
                    id: backMa; anchors.fill: parent
                    onClicked: main.settingsOpen = false
                }
            }
        }
    }

    Flickable {
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        clip: true
        contentWidth: width
        contentHeight: settingsColumn.height + 40

        Column {
            id: settingsColumn
            x: Math.max(0, (parent.width - width) / 2)
            width: Math.min(parent.width, 700)
            spacing: 20

            Rectangle {
                width: parent.width; height: 130
                color: Theme.bgPanel
                border.color: Theme.border
                border.width: 1
                radius: 8

                Column {
                    x: 16; y: 12
                    spacing: 10
                    width: parent.width - 32

                    Text {
                        text: "仿真引擎"
                        color: Theme.textStrong
                        font.pixelSize: Theme.fsLarge
                        font.bold: true
                    }
                    Text {
                        text: "新引擎支持门延迟、导线延迟、事件驱动、振荡器仿真"
                        color: Theme.textGray
                        font.pixelSize: Theme.fsSmall
                        width: parent.width
                        wrapMode: Text.Wrap
                    }

                    Row {
                        spacing: 16
                        RadioButton {
                            id: rbEvent
                            text: "事件驱动引擎（新）"
                            checked: settingsPage.engineIndex === 0
                            onToggled: if (checked) settingsPage.engineIndex = 0
                            contentItem: Text {
                                text: rbEvent.text
                                color: Theme.text
                                font.pixelSize: Theme.fsNormal
                                leftPadding: rbEvent.indicator.width + 6
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        RadioButton {
                            id: rbIter
                            text: "迭代引擎（旧）"
                            checked: settingsPage.engineIndex === 1
                            onToggled: if (checked) settingsPage.engineIndex = 1
                            contentItem: Text {
                                text: rbIter.text
                                color: Theme.text
                                font.pixelSize: Theme.fsNormal
                                leftPadding: rbIter.indicator.width + 6
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: settingsPage.engineIndex === 0 ? 210 : 0
                visible: settingsPage.engineIndex === 0
                color: Theme.bgPanel
                border.color: Theme.border
                border.width: 1
                radius: 8

                Column {
                    x: 16; y: 12
                    spacing: 10
                    width: parent.width - 32

                    Text {
                        text: "传播延迟"
                        color: Theme.textStrong
                        font.pixelSize: Theme.fsLarge
                        font.bold: true
                    }
                    Text {
                        text: "延迟使用逻辑时间单位（不是真实时间）。\n门延迟作用于所有逻辑门，导线延迟作用于两个门之间的连线（与长度无关）。"
                        color: Theme.textGray
                        font.pixelSize: Theme.fsSmall
                        width: parent.width
                        wrapMode: Text.Wrap
                    }

                    Row {
                        spacing: 8
                        Text {
                            text: "全局门延迟"
                            color: Theme.text
                            font.pixelSize: Theme.fsNormal
                            width: 120
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Rectangle {
                            width: 160; height: 30
                            color: Theme.bgInput
                            border.color: Theme.borderStrong
                            border.width: 1; radius: Theme.smallRadius
                            TextField {
                                id: gateDelayField
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: Theme.text
                                font.pixelSize: Theme.fsNormal
                                background: null
                                selectByMouse: true
                                text: settingsPage.gateDelayText
                                validator: RegularExpressionValidator { regularExpression: /[0-9]{0,18}/ }
                                onTextChanged: settingsPage.gateDelayText = text
                            }
                        }
                        Text {
                            text: "（默认 1）"
                            color: Theme.textFaint
                            font.pixelSize: Theme.fsSmall
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Row {
                        spacing: 8
                        Text {
                            text: "导线传播延迟"
                            color: Theme.text
                            font.pixelSize: Theme.fsNormal
                            width: 120
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Rectangle {
                            width: 160; height: 30
                            color: Theme.bgInput
                            border.color: Theme.borderStrong
                            border.width: 1; radius: Theme.smallRadius
                            TextField {
                                id: wireDelayField
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: Theme.text
                                font.pixelSize: Theme.fsNormal
                                background: null
                                selectByMouse: true
                                text: settingsPage.wireDelayText
                                validator: RegularExpressionValidator { regularExpression: /[0-9]{0,18}/ }
                                onTextChanged: settingsPage.wireDelayText = text
                            }
                        }
                        Text {
                            text: "（默认 1）"
                            color: Theme.textFaint
                            font.pixelSize: Theme.fsSmall
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: settingsPage.engineIndex === 0 ? 190 : 0
                visible: settingsPage.engineIndex === 0
                color: Theme.bgPanel
                border.color: Theme.border
                border.width: 1
                radius: 8

                Column {
                    x: 16; y: 12
                    spacing: 10
                    width: parent.width - 32

                    Text {
                        text: "仿真速度"
                        color: Theme.textStrong
                        font.pixelSize: Theme.fsLarge
                        font.bold: true
                    }
                    Text {
                        text: "每帧推进的逻辑时间单位数。数值可以很大。"
                        color: Theme.textGray
                        font.pixelSize: Theme.fsSmall
                        width: parent.width
                        wrapMode: Text.Wrap
                    }

                    CheckBox {
                        id: autoSpeedBox
                        text: "自动"
                        checked: settingsPage.autoSpeed
                        onToggled: settingsPage.autoSpeed = checked
                        contentItem: Text {
                            text: autoSpeedBox.text
                            color: Theme.text
                            font.pixelSize: Theme.fsNormal
                            leftPadding: autoSpeedBox.indicator.width + 6
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Row {
                        spacing: 8
                        enabled: !settingsPage.autoSpeed
                        opacity: settingsPage.autoSpeed ? 0.5 : 1

                        Text {
                            text: "速度 = 1 × 10^"
                            color: Theme.text
                            font.pixelSize: Theme.fsNormal
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Rectangle {
                            width: 80; height: 30
                            color: Theme.bgInput
                            border.color: Theme.borderStrong
                            border.width: 1; radius: Theme.smallRadius
                            TextField {
                                id: speedExpField
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: Theme.text
                                font.pixelSize: Theme.fsNormal
                                background: null
                                selectByMouse: true
                                text: String(settingsPage.speedExp)
                                validator: IntValidator { bottom: 0; top: 18 }
                                onTextChanged: {
                                    var n = parseInt(text)
                                    if (!isNaN(n)) settingsPage.speedExp = n
                                }
                            }
                        }
                        Text {
                            text: "时间单位/秒"
                            color: Theme.textGray
                            font.pixelSize: Theme.fsSmall
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            Text {
                visible: settingsPage.errorText.length > 0
                text: "⚠ " + settingsPage.errorText
                color: Theme.accentRed
                font.pixelSize: Theme.fsSmall
                width: parent.width
                wrapMode: Text.Wrap
            }

            Row {
                spacing: 12

                Rectangle {
                    width: 120; height: 36
                    radius: Theme.buttonRadius
                    color: saveSettingsMa.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                    border.color: Theme.borderSuccess; border.width: 1
                    Text {
                        anchors.centerIn: parent; text: "应用并返回"
                        color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                    }
                    MouseArea {
                        id: saveSettingsMa; anchors.fill: parent
                        onClicked: { settingsPage.apply(); main.settingsOpen = false }
                    }
                }

                Rectangle {
                    width: 120; height: 36
                    radius: Theme.buttonRadius
                    color: resetSettingsMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                    border.color: Theme.borderStrong; border.width: 1
                    Text {
                        anchors.centerIn: parent; text: "恢复默认"
                        color: Theme.text; font.pixelSize: Theme.fsNormal
                    }
                    MouseArea {
                        id: resetSettingsMa; anchors.fill: parent
                        onClicked: settingsPage.resetToDefaults()
                    }
                }
            }
        }
    }

    function apply() {
        errorText = ""

        var newEngine = (engineIndex === 0) ? "event" : "iter"
        if (newEngine !== main.circuitRef.engineType) {
            main.circuitRef.engineType = newEngine
        }

        if (engineIndex === 0) {
            var gd = parseInt(gateDelayText)
            if (isNaN(gd) || gd < 1) { errorText = "门延迟必须 >= 1"; return }
            main.circuitRef.globalGateDelay = gd

            var wd = parseInt(wireDelayText)
            if (isNaN(wd) || wd < 1) { errorText = "导线延迟必须 >= 1"; return }
            main.circuitRef.wireDelay = wd

            main.circuitRef.autoSpeed = autoSpeed
            if (!autoSpeed) {
                main.circuitRef.simulationSpeedExp = speedExp
            }
        }

        main.statusText = "设置已应用"
    }

    function resetToDefaults() {
        gateDelayText = "1"
        wireDelayText = "1"
        speedExp = 3
        autoSpeed = true
    }
}
