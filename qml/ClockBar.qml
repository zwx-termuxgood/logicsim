import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Rectangle {
    id: clockBar
    property QtObject main: null

    anchors.top: parent.top
    anchors.left: parent.left
    anchors.right: parent.right
    height: Theme.clockBarH
    color: Theme.bgBar
    border.color: Theme.border
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 6

        Rectangle {
            Layout.preferredWidth: 80; Layout.preferredHeight: 26; radius: Theme.smallRadius
            color: playBtn.pressed ? Theme.bgButtonRunHi
            : (main.clockRunning ? Theme.bgButtonRun : Theme.bgButton)
            border.color: main.clockRunning ? Theme.accentGreen : Theme.borderStrong
            border.width: 1
            Text {
                anchors.centerIn: parent
                text: main.clockRunning ? "⏸ 停止" : "▶ 运行"
                color: Theme.text; font.pixelSize: Theme.fsSmall
            }
            MouseArea { id: playBtn; anchors.fill: parent
                onClicked: {
                    if (main.clockRunning) main.circuitRef.stopClock()
                        else main.circuitRef.startClock()
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 60; Layout.preferredHeight: 26; radius: Theme.smallRadius
            color: stepBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
            border.color: Theme.borderStrong; border.width: 1
            Text { anchors.centerIn: parent; text: "单步"
                color: Theme.text; font.pixelSize: Theme.fsSmall }
                MouseArea { id: stepBtn; anchors.fill: parent
                    onClicked: main.circuitRef.singleStep() }
        }

        Rectangle {
            Layout.preferredWidth: 60; Layout.preferredHeight: 26; radius: Theme.smallRadius
            color: resetBtn.pressed ? Theme.bgButtonResetHi : Theme.bgButtonReset
            border.color: Theme.borderDanger; border.width: 1
            Text { anchors.centerIn: parent; text: "复位"
                color: Theme.textWhite; font.pixelSize: Theme.fsSmall }
                MouseArea { id: resetBtn; anchors.fill: parent
                    onClicked: {
                        main.circuitRef.stopClock()
                        main.circuitRef.resetAllInputs()
                    }
                }
        }

        Rectangle { width: 1; height: 22; color: Theme.borderStrong }

        Text { text: "频率"; color: Theme.textGray; font.pixelSize: Theme.fsSmall }
        SpinBox {
            id: freqSpin
            from: 1; to: 1000; editable: true
            Layout.preferredWidth: 100
            value: main.clockFrequency
            onValueChanged: main.circuitRef.setClockFrequency(value)
        }
        Text { text: "Hz"; color: Theme.textGray; font.pixelSize: Theme.fsSmall }

        Rectangle { width: 1; height: 22; color: Theme.borderStrong }

        Text {
            text: "拍数：" + main.clockTickCount
            color: Theme.text; font.pixelSize: Theme.fsSmall
        }

        Item { Layout.fillWidth: true }
    }
}
