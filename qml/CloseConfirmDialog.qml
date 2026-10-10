import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Dialog {
    id: closeConfirm
    property QtObject main: null

    anchors.centerIn: parent
    modal: true
    title: "未保存"
    standardButtons: Dialog.NoButton
    padding: 20
    width: 340

    background: Rectangle {
        color: Theme.bgDialog
        border.color: Theme.borderDialog
        border.width: 1
        radius: Theme.dialogRadius
    }

    contentItem: ColumnLayout {
        spacing: 12

        Text {
            text: "电路已修改，是否保存？"
            color: Theme.textStrong
            font.pixelSize: Theme.fsLarge
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: 80; Layout.preferredHeight: 32
                radius: Theme.buttonRadius
                color: cc1.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderDialog; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "取消"
                    color: Theme.text; font.pixelSize: Theme.fsNormal
                }
                MouseArea { id: cc1; anchors.fill: parent
                    onClicked: closeConfirm.close() }
            }
            Rectangle {
                Layout.preferredWidth: 100; Layout.preferredHeight: 32
                radius: Theme.buttonRadius
                color: cc2.pressed ? Theme.bgButtonDangerHi : Theme.bgButtonDanger
                border.color: Theme.borderDangerHi; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "不保存"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                }
                MouseArea { id: cc2; anchors.fill: parent
                    onClicked: {
                        main.dirty = false
                        main.quitApp()
                    } }
            }
            Rectangle {
                Layout.preferredWidth: 80; Layout.preferredHeight: 32
                radius: Theme.buttonRadius
                color: cc3.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                border.color: Theme.borderSuccess; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "保存"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                }
                MouseArea { id: cc3; anchors.fill: parent
                    onClicked: {
                        closeConfirm.close()
                        main.saveBeforeExit = true
                        if (main.currentFilePath !== "") {
                            if (main.circuitRef.saveToFile(main.currentFilePath)) {
                                main.dirty = false
                                main.saveBeforeExit = false
                                main.quitApp()
                            } else {
                                main.saveBeforeExit = false
                                main.saveDialogRef.open()
                            }
                        } else {
                            main.saveDialogRef.open()
                        }
                    } }
            }
        }
    }
}