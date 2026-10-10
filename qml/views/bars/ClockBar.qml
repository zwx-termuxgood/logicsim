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

    Flickable {
        id: clockFlick
        anchors.fill: parent
        contentWidth: clockRow.implicitWidth + 16
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Row {
            id: clockRow
            x: 8
            y: 0
            height: parent.height
            spacing: 6

            Row {
                visible: main.engineType === "event"
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    width: 96; height: 26; radius: Theme.smallRadius
                    color: simStartMa.pressed ? Theme.bgButtonRunHi
                           : (main.simulationRunning ? Theme.bgButtonRun : Theme.bgButton)
                    border.color: main.simulationRunning ? Theme.accentGreen : Theme.borderStrong
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: main.simulationRunning ? "⏸ 暂停仿真" : "▶ 开始仿真"
                        color: Theme.text; font.pixelSize: Theme.fsSmall
                    }
                    MouseArea {
                        id: simStartMa
                        anchors.fill: parent
                        onClicked: {
                            if (main.simulationRunning) main.circuitRef.pauseSimulation()
                            else                        main.circuitRef.startSimulation()
                        }
                    }
                }

                Rectangle {
                    width: 60; height: 26; radius: Theme.smallRadius
                    color: simStepMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                    border.color: Theme.borderStrong; border.width: 1
                    Text { anchors.centerIn: parent; text: "单步"
                        color: Theme.text; font.pixelSize: Theme.fsSmall }
                    MouseArea {
                        id: simStepMa; anchors.fill: parent
                        onClicked: main.circuitRef.simulateStep()
                    }
                }

                Rectangle {
                    width: 60; height: 26; radius: Theme.smallRadius
                    color: simResetMa.pressed ? Theme.bgButtonResetHi : Theme.bgButtonReset
                    border.color: Theme.borderDanger; border.width: 1
                    Text { anchors.centerIn: parent; text: "复位"
                        color: Theme.textWhite; font.pixelSize: Theme.fsSmall }
                    MouseArea {
                        id: simResetMa; anchors.fill: parent
                        onClicked: main.circuitRef.resetSimulation()
                    }
                }

                Rectangle { width: 1; height: 22; color: Theme.borderStrong }

                Text {
                    text: "仿真时间：" + main.circuitRef.simTime
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle { width: 1; height: 22; color: Theme.borderStrong }
            }

            Row {
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    width: 80; height: 26; radius: Theme.smallRadius
                    color: playBtn.pressed ? Theme.bgButtonRunHi
                           : (main.clockRunning ? Theme.bgButtonRun : Theme.bgButton)
                    border.color: main.clockRunning ? Theme.accentGreen : Theme.borderStrong
                    border.width: 1
                    opacity: (main.engineType === "event" && !main.simulationRunning) ? 0.5 : 1
                    Text {
                        anchors.centerIn: parent
                        text: main.clockRunning ? "⏸ 停止" : "▶ 时钟"
                        color: Theme.text; font.pixelSize: Theme.fsSmall
                    }
                    MouseArea {
                        id: playBtn; anchors.fill: parent
                        enabled: !(main.engineType === "event" && !main.simulationRunning)
                        onClicked: {
                            if (main.clockRunning) main.circuitRef.stopClock()
                            else                    main.circuitRef.startClock()
                        }
                    }
                }

                Rectangle {
                    width: 64; height: 26; radius: Theme.smallRadius
                    color: stepBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
                    border.color: Theme.borderStrong; border.width: 1
                    Text { anchors.centerIn: parent; text: "时钟拍"
                        color: Theme.text; font.pixelSize: Theme.fsSmall }
                    MouseArea { id: stepBtn; anchors.fill: parent
                        onClicked: main.circuitRef.singleStep() }
                }

                Rectangle {
                    width: 68; height: 26; radius: Theme.smallRadius
                    color: resetBtn.pressed ? Theme.bgButtonResetHi : Theme.bgButtonReset
                    border.color: Theme.borderDanger; border.width: 1
                    Text { anchors.centerIn: parent; text: "复位输入"
                        color: Theme.textWhite; font.pixelSize: Theme.fsSmall }
                    MouseArea { id: resetBtn; anchors.fill: parent
                        onClicked: {
                            main.circuitRef.stopClock()
                            main.circuitRef.resetAllInputs()
                        }
                    }
                }

                Rectangle { width: 1; height: 22; color: Theme.borderStrong }

                Text { text: "频率"; color: Theme.textGray; font.pixelSize: Theme.fsSmall
                    anchors.verticalCenter: parent.verticalCenter }
                SpinBox {
                    id: freqSpin
                    from: 1; to: 1000; editable: true
                    width: 100
                    value: main.clockFrequency
                    anchors.verticalCenter: parent.verticalCenter
                    onValueChanged: main.circuitRef.setClockFrequency(value)
                }
                Text { text: "Hz"; color: Theme.textGray; font.pixelSize: Theme.fsSmall
                    anchors.verticalCenter: parent.verticalCenter }

                Rectangle { width: 1; height: 22; color: Theme.borderStrong }

                Text {
                    text: "拍数：" + main.clockTickCount
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                    anchors.verticalCenter: parent.verticalCenter
                }

                Item { width: 8; height: 1 }
            }
        }
    }
}
