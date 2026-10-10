import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Popup {
    id: recentPopup
    property QtObject main: null

    anchors.centerIn: parent
    width: 380
    height: Math.min(400, 44 * Math.max(1, main.recentList.length) + 70)
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: Theme.bgDialog
        border.color: Theme.borderDialog
        border.width: 1
        radius: 8
    }

    contentItem: Column {
        spacing: 0

        Rectangle {
            width: recentPopup.width
            height: 32
            color: Theme.bgDialogHeader

            Text {
                anchors.left: parent.left; anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: "最近打开"
                color: Theme.textGray
                font.pixelSize: Theme.fsNormal
                font.bold: true
            }
            Rectangle {
                anchors.right: parent.right; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 60; height: 22; radius: Theme.smallRadius
                color: clearRecentMa.pressed ? "#7a2020" : Theme.bgButtonDanger
                border.color: Theme.borderDanger; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "清空"
                    color: Theme.textWhite; font.pixelSize: Theme.fsTiny
                }
                MouseArea {
                    id: clearRecentMa; anchors.fill: parent
                    enabled: main.recentList.length > 0
                    onClicked: main.circuitRef.clearRecentFiles()
                }
            }
        }

        Text {
            visible: main.recentList.length === 0
            width: recentPopup.width; height: 60
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "暂无最近打开的文件"
            color: Theme.textMuted
            font.pixelSize: Theme.fsNormal
        }

        Repeater {
            model: main.recentList
            delegate: Rectangle {
                width: recentPopup.width; height: 44
                color: itemMa.pressed ? Theme.bgButtonHover : "transparent"

                Column {
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.right: delMaBox.left; anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: modelData.name
                        color: Theme.text
                        font.pixelSize: Theme.fsNormal
                        elide: Text.ElideRight
                        width: parent.width
                    }
                    Text {
                        text: modelData.time + "  " + modelData.path
                        color: Theme.textFaint
                        font.pixelSize: Theme.fsTiny
                        elide: Text.ElideMiddle
                        width: parent.width
                    }
                }

                Rectangle {
                    id: delMaBox
                    anchors.right: parent.right; anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24; height: 24; radius: Theme.smallRadius
                    color: delMaMouse.pressed ? "#7a2020" : Theme.bgButtonDanger
                    Text {
                        anchors.centerIn: parent; text: "×"
                        color: Theme.textWhite; font.pixelSize: 14
                    }
                    MouseArea {
                        id: delMaMouse; anchors.fill: parent
                        onClicked: main.circuitRef.removeRecentFile(modelData.path)
                    }
                }

                MouseArea {
                    id: itemMa
                    anchors.fill: parent
                    anchors.rightMargin: 40
                    onClicked: {
                        if (main.circuitRef.loadFromFile(modelData.path)) {
                            main.currentFilePath = modelData.path
                            main.dirty = false
                            main.selectedCompId = ""
                            main.selectedComp = null
                            main.selectedIds = []
                            main.resetView()
                            main.statusText = "已打开：" + modelData.name
                        } else {
                            main.statusText = "打开失败：" + modelData.path
                        }
                        recentPopup.close()
                    }
                }
            }
        }

        Item { width: 1; height: 8 }
    }
}