import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Dialog {
    id: confirmDialog
    property QtObject main: null

    property string message: ""
    property var action: null

    anchors.centerIn: parent
    modal: true
    standardButtons: Dialog.NoButton
    width: 320
    padding: 20

    background: Rectangle {
        color: Theme.bgDialog
        border.color: Theme.borderDialog
        border.width: 1
        radius: Theme.dialogRadius
    }

    contentItem: ColumnLayout {
        spacing: 12

        Text {
            text: confirmDialog.title
            color: Theme.textStrong
            font.pixelSize: Theme.fsLarge
            font.bold: true
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Text {
            text: confirmDialog.message
            color: Theme.textGray
            font.pixelSize: Theme.fsNormal
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: 70; Layout.preferredHeight: 30
                radius: Theme.buttonRadius
                color: cd1.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderDialog; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "取消"
                    color: Theme.text; font.pixelSize: Theme.fsNormal
                }
                MouseArea { id: cd1; anchors.fill: parent
                    onClicked: confirmDialog.close() }
            }
            Rectangle {
                Layout.preferredWidth: 80; Layout.preferredHeight: 30
                radius: Theme.buttonRadius
                color: cd2.pressed ? Theme.bgButtonDangerHi : Theme.bgButtonDanger
                border.color: Theme.borderDangerHi; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "确定"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                }
                MouseArea { id: cd2; anchors.fill: parent
                    onClicked: {
                        if (confirmDialog.action) confirmDialog.action()
                        confirmDialog.close()
                    } }
            }
        }
    }
}