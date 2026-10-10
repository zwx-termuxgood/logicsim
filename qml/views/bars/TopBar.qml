import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Rectangle {
    id: topBar
    property QtObject main: null

    property alias ctxBtn: ctxBtn
    property alias menuBtn: menuBtn

    anchors.top: parent.top
    anchors.left: parent.left
    anchors.right: parent.right
    height: Theme.topBarH
    color: Theme.bgPanel
    border.color: Theme.border
    border.width: 1

    Flickable {
        id: topFlick
        anchors.fill: parent
        contentWidth: topRow.implicitWidth + 12
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Row {
            id: topRow
            x: 6
            y: 0
            height: parent.height
            spacing: 3

            Text {
                text: "LogicSim"
                color: Theme.textStrong
                font.pixelSize: Theme.fsLarge
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                width: 1; height: 22; color: Theme.borderStrong
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {
                id: menuBtn
                width: 52; height: 28
                radius: Theme.buttonRadius
                color: menuBtnMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderStrong; border.width: 1
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    anchors.centerIn: parent; text: "菜单"
                    color: Theme.text; font.pixelSize: Theme.fsSmall
                }
                MouseArea {
                    id: menuBtnMa
                    anchors.fill: parent
                    onClicked: main.menuPopupRef.open()
                }
            }

            Rectangle {
                width: 1; height: 22; color: Theme.borderStrong
                anchors.verticalCenter: parent.verticalCenter
            }

            Repeater {
                model: [ { id: "edit", name: "编辑" },
                         { id: "control", name: "控制" },
                         { id: "select", name: "选择" } ]
                delegate: Rectangle {
                    width: 44; height: 28
                    radius: Theme.buttonRadius
                    anchors.verticalCenter: parent.verticalCenter
                    color: main.mode === modelData.id ? Theme.bgButtonActive : Theme.bgButton
                    border.color: main.mode === modelData.id ? Theme.borderBtnActive : Theme.borderWeak
                    border.width: 1
                    opacity: main.circuitViewOnly ? 0.5 : 1

                    Text {
                        anchors.centerIn: parent
                        text: modelData.name
                        color: main.mode === modelData.id ? Theme.textWhite : Theme.textGray
                        font.pixelSize: Theme.fsSmall
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !main.circuitViewOnly
                        onClicked: {
                            main.mode = modelData.id
                            main.wiring = false
                            main.placingType = ""
                            if (modelData.id !== "select") main.selectedIds = []
                        }
                    }
                }
            }

            Rectangle {
                width: 1; height: 22; color: Theme.borderStrong
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {
                width: 28; height: 28
                radius: Theme.buttonRadius
                anchors.verticalCenter: parent.verticalCenter
                color: undoBtn.pressed && main.circuitRef.canUndo
                       ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderStrong; border.width: 1
                opacity: main.circuitRef.canUndo ? 1 : 0.4
                Text { anchors.centerIn: parent; text: "↶"
                    color: Theme.text; font.pixelSize: 15 }
                MouseArea {
                    id: undoBtn; anchors.fill: parent
                    enabled: main.circuitRef.canUndo && !main.circuitViewOnly
                    onClicked: main.circuitRef.undo()
                }
            }
            Rectangle {
                width: 28; height: 28
                radius: Theme.buttonRadius
                anchors.verticalCenter: parent.verticalCenter
                color: redoBtn.pressed && main.circuitRef.canRedo
                       ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderStrong; border.width: 1
                opacity: main.circuitRef.canRedo ? 1 : 0.4
                Text { anchors.centerIn: parent; text: "↷"
                    color: Theme.text; font.pixelSize: 15 }
                MouseArea {
                    id: redoBtn; anchors.fill: parent
                    enabled: main.circuitRef.canRedo && !main.circuitViewOnly
                    onClicked: main.circuitRef.redo()
                }
            }

            Rectangle {
                width: 1; height: 22; color: Theme.borderStrong
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {
                width: 28; height: 28
                radius: Theme.buttonRadius
                anchors.verticalCenter: parent.verticalCenter
                color: zo.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderStrong; border.width: 1
                Text { anchors.centerIn: parent; text: "−"
                    color: Theme.text; font.pixelSize: 15 }
                MouseArea { id: zo; anchors.fill: parent
                    onClicked: main.zoomOut() }
            }
            Rectangle {
                width: 46; height: 28
                radius: Theme.buttonRadius
                anchors.verticalCenter: parent.verticalCenter
                color: rst.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderStrong; border.width: 1
                Text { anchors.centerIn: parent
                    text: Math.round(main.viewScale * 100) + "%"
                    color: Theme.text; font.pixelSize: Theme.fsTiny }
                MouseArea { id: rst; anchors.fill: parent
                    onClicked: main.resetView() }
            }
            Rectangle {
                width: 28; height: 28
                radius: Theme.buttonRadius
                anchors.verticalCenter: parent.verticalCenter
                color: zi.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderStrong; border.width: 1
                Text { anchors.centerIn: parent; text: "+"
                    color: Theme.text; font.pixelSize: 15 }
                MouseArea { id: zi; anchors.fill: parent
                    onClicked: main.zoomIn() }
            }

            Rectangle {
                width: 1; height: 22; color: Theme.borderStrong
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {
                id: ctxBtn
                width: 150; height: 28
                radius: Theme.buttonRadius
                anchors.verticalCenter: parent.verticalCenter
                color: ctxBtnMouse.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderDialog; border.width: 1
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 6; spacing: 4
                    Text {
                        Layout.fillWidth: true
                        text: main.circuitRef.contextName
                        color: Theme.text; font.pixelSize: Theme.fsSmall
                        elide: Text.ElideRight
                    }
                    Text { text: "▼"; color: Theme.textWeak; font.pixelSize: 9 }
                }
                MouseArea { id: ctxBtnMouse; anchors.fill: parent
                    onClicked: main.contextPopupRef.open() }
            }

            Text {
                visible: main.dirty
                text: "●"
                color: Theme.accentDirty
                font.pixelSize: 14
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {
                width: 28; height: 28
                radius: Theme.buttonRadius
                anchors.verticalCenter: parent.verticalCenter
                color: collapseMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderStrong; border.width: 1
                Text { anchors.centerIn: parent; text: "▲"
                    color: Theme.text; font.pixelSize: 13 }
                MouseArea {
                    id: collapseMa
                    anchors.fill: parent
                    onClicked: main.topCollapsed = true
                }
            }

            Item { width: 8; height: 1 }
        }
    }
}
