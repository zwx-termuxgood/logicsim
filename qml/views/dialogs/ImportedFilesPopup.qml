import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Popup {
    id: importedPopup
    property QtObject main: null

    property var items: []

    anchors.centerIn: parent
    width: 440
    height: Math.min(420, 60 * Math.max(1, items.length) + 90)
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
            width: importedPopup.width
            height: 34
            color: Theme.bgDialogHeader
            Text {
                anchors.left: parent.left; anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: "已导入的文件"
                color: Theme.textGray
                font.pixelSize: Theme.fsNormal
                font.bold: true
            }
        }

        Text {
            visible: importedPopup.items.length === 0
            width: importedPopup.width; height: 80
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "暂无导入的文件"
            color: Theme.textMuted
            font.pixelSize: Theme.fsNormal
        }

        Repeater {
            model: importedPopup.items
            delegate: Rectangle {
                width: importedPopup.width
                height: 56
                color: "transparent"

                Column {
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.right: delBtn.left; anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        text: modelData.baseName + "  (" + modelData.subCount + " 个电路)"
                        color: Theme.text
                        font.pixelSize: Theme.fsNormal
                        elide: Text.ElideRight
                        width: parent.width
                    }
                    Text {
                        text: modelData.path
                        color: Theme.textFaint
                        font.pixelSize: Theme.fsTiny
                        elide: Text.ElideMiddle
                        width: parent.width
                    }
                }

                Rectangle {
                    id: delBtn
                    anchors.right: parent.right; anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 68; height: 26; radius: Theme.smallRadius
                    color: delMa.pressed ? "#7a2020" : Theme.bgButtonDanger
                    border.color: Theme.borderDanger; border.width: 1
                    Text {
                        anchors.centerIn: parent; text: "取消导入"
                        color: Theme.textWhite; font.pixelSize: Theme.fsTiny
                    }
                    MouseArea {
                        id: delMa; anchors.fill: parent
                        onClicked: {
                            if (importedPopup.main.circuitRef.removeImportedFile(modelData.path)) {
                                importedPopup.main.statusText = "已取消导入：" + modelData.baseName
                                importedPopup.main.dirty = true
                                importedPopup.refresh()
                            }
                        }
                    }
                }
            }
        }

        Item { width: 1; height: 8 }
    }

    onOpened: refresh()

    function refresh() {
        items = main ? main.circuitRef.importedFiles() : []
    }
}