import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Dialog {
    id: exitConfirm
    property QtObject main: null

    anchors.centerIn: parent
    modal: true
    title: "退出"
    standardButtons: Dialog.NoButton
    padding: 20
    width: 320

    background: Rectangle {
        color: Theme.bgDialog
        border.color: Theme.borderDialog
        border.width: 1
        radius: Theme.dialogRadius
    }

    contentItem: ColumnLayout {
        spacing: 12

        Text {
            text: "确定要退出 LogicSim 吗？"
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
                color: ec1.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderDialog; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "取消"
                    color: Theme.text; font.pixelSize: Theme.fsNormal
                }
                MouseArea { id: ec1; anchors.fill: parent
                    onClicked: exitConfirm.close() }
            }
            Rectangle {
                Layout.preferredWidth: 80; Layout.preferredHeight: 32
                radius: Theme.buttonRadius
                color: ec2.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                border.color: Theme.borderSuccess; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "退出"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                }
                MouseArea { id: ec2; anchors.fill: parent
                    onClicked: main.quitApp() }
            }
        }
    }
}
