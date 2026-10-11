import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Dialog {
    id: unsavedDialog
    property QtObject main: null

    // 用户点"继续"后要执行的动作（打开新文件 / 清空 / …）
    property var pendingAction: null

    anchors.centerIn: parent
    modal: true
    title: "未保存"
    standardButtons: Dialog.NoButton
    padding: 20
    width: 360

    background: Rectangle {
        color: Theme.bgDialog
        border.color: Theme.borderDialog
        border.width: 1
        radius: Theme.dialogRadius
    }

    contentItem: ColumnLayout {
        spacing: 12

        Text {
            text: "当前电路已修改，是否保存？"
            color: Theme.textStrong
            font.pixelSize: Theme.fsLarge
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: 70; Layout.preferredHeight: 32
                radius: Theme.buttonRadius
                color: b1.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderDialog; border.width: 1
                Text { anchors.centerIn: parent; text: "取消"
                    color: Theme.text; font.pixelSize: Theme.fsNormal }
                MouseArea { id: b1; anchors.fill: parent
                    onClicked: unsavedDialog.close() }
            }
            Rectangle {
                Layout.preferredWidth: 90; Layout.preferredHeight: 32
                radius: Theme.buttonRadius
                color: b2.pressed ? Theme.bgButtonDangerHi : Theme.bgButtonDanger
                border.color: Theme.borderDangerHi; border.width: 1
                Text { anchors.centerIn: parent; text: "不保存"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal }
                MouseArea { id: b2; anchors.fill: parent
                    onClicked: {
                        unsavedDialog.close()
                        unsavedDialog.main.dirty = false
                        var act = unsavedDialog.pendingAction
                        unsavedDialog.pendingAction = null
                        if (act) act()
                    } }
            }
            Rectangle {
                Layout.preferredWidth: 80; Layout.preferredHeight: 32
                radius: Theme.buttonRadius
                color: b3.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                border.color: Theme.borderSuccess; border.width: 1
                Text { anchors.centerIn: parent; text: "保存"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal }
                MouseArea { id: b3; anchors.fill: parent
                    onClicked: {
                        unsavedDialog.close()
                        // 当前路径已知：直接保存，成功后执行 pendingAction
                        // 未知路径：弹另存为对话框，把 pendingAction 交给 saveDialog 处理
                        var act = unsavedDialog.pendingAction
                        unsavedDialog.pendingAction = null

                        if (unsavedDialog.main.currentFilePath !== "") {
                            if (unsavedDialog.main.circuitRef.saveToFile(
                                        unsavedDialog.main.currentFilePath)) {
                                unsavedDialog.main.dirty = false
                                if (act) act()
                            } else {
                                unsavedDialog.main.statusText = "保存失败"
                            }
                        } else {
                            // 让 saveDialog 保存成功后执行 act
                            unsavedDialog.main.pendingAfterSave = act
                            unsavedDialog.main.saveDialogRef.open()
                        }
                    } }
            }
        }
    }
}
