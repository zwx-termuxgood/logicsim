import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Dialog {
    id: subNameDialog
    property QtObject main: null

    anchors.centerIn: parent
    modal: true
    title: "新建子电路"
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
            text: "输入子电路名称"
            color: Theme.textStrong
            font.pixelSize: Theme.fsMedium
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: Theme.bgInput
            border.color: Theme.borderDialog
            border.width: 1
            radius: Theme.buttonRadius

            TextField {
                id: subNameInput
                anchors.fill: parent
                anchors.leftMargin: 8; anchors.rightMargin: 8
                color: Theme.text
                font.pixelSize: Theme.fsNormal
                background: null
                selectByMouse: true
                onAccepted: subNameDialog.doCreate()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: 70; Layout.preferredHeight: 30
                radius: Theme.buttonRadius
                color: sn1.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderDialog; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "取消"
                    color: Theme.text; font.pixelSize: Theme.fsNormal
                }
                MouseArea {
                    id: sn1; anchors.fill: parent
                    onClicked: subNameDialog.close()
                }
            }
            Rectangle {
                Layout.preferredWidth: 80; Layout.preferredHeight: 30
                radius: Theme.buttonRadius
                color: sn2.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                border.color: Theme.borderSuccess; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "创建"
                    color: Theme.textWhite; font.pixelSize: Theme.fsNormal
                }
                MouseArea {
                    id: sn2; anchors.fill: parent
                    onClicked: subNameDialog.doCreate()
                }
            }
        }
    }

    function doCreate() {
        if (subNameInput.text.trim().length === 0) return
            main.circuitRef.addSubcircuit(subNameInput.text.trim())
            close()
    }

    onOpened: {
        subNameInput.text = "子电路"
        subNameInput.forceActiveFocus()
        subNameInput.selectAll()
    }
}
