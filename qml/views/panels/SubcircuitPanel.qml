import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Column {
    id: subcircuitPanel
    property QtObject main: null

    spacing: 5
    width: parent ? parent.width : 200

    function findSubInfo(subId) {
        if (!main) return null
        if (!main.subList) return null
        for (var i = 0; i < main.subList.length; ++i) {
            if (main.subList[i].id === subId) return main.subList[i]
        }
        return null
    }

    readonly property string selSubId: main ? (main.selectedSubId || "") : ""
    readonly property bool viewOnly: main ? main.circuitViewOnly : false

    Rectangle {
        width: parent.width; height: 24; radius: Theme.smallRadius
        color: Theme.bgDialogHeader
        Text {
            anchors.left: parent.left; anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: "子电路"
            color: Theme.accentPurple
            font.pixelSize: Theme.fsSmall; font.bold: true
        }
    }

    Row {
        spacing: 4
        Text {
            text: "名称"; color: Theme.textGray
            font.pixelSize: Theme.fsSmall; width: 36
            anchors.verticalCenter: parent.verticalCenter
        }
        Rectangle {
            width: subcircuitPanel.width - 42; height: 26
            color: Theme.bgInput
            border.color: Theme.borderStrong; border.width: 1
            radius: Theme.smallRadius
            TextField {
                id: subNameField
                anchors.fill: parent
                anchors.leftMargin: 5; anchors.rightMargin: 5
                color: Theme.text; font.pixelSize: Theme.fsSmall
                background: null
                selectByMouse: true
                readOnly: subcircuitPanel.viewOnly
                text: {
                    var info = subcircuitPanel.findSubInfo(subcircuitPanel.selSubId)
                    return info ? info.name : ""
                }
                onEditingFinished: {
                    if (!main) return
                    if (subcircuitPanel.selSubId === "" || subcircuitPanel.viewOnly) return
                    if (text.trim().length === 0) return
                    main.circuitRef.setSubcircuitName(subcircuitPanel.selSubId, text.trim())
                }
            }
        }
    }

    Text {
        text: {
            var info = subcircuitPanel.findSubInfo(subcircuitPanel.selSubId)
            if (!info) return ""
            return "ID：" + info.id + "\n输入端口：" + info.inCount
                 + "    输出端口：" + info.outCount
        }
        color: Theme.textGray
        font.pixelSize: Theme.fsSmall
        wrapMode: Text.Wrap
        width: parent.width
    }

    Rectangle {
        width: parent.width; height: 30; radius: Theme.smallRadius
        visible: !subcircuitPanel.viewOnly
        color: renameBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
        border.color: Theme.borderStrong; border.width: 1
        Text {
            anchors.centerIn: parent; text: "重命名…"
            color: Theme.text; font.pixelSize: Theme.fsSmall
        }
        MouseArea {
            id: renameBtn; anchors.fill: parent
            onClicked: {
                if (!main) return
                var info = subcircuitPanel.findSubInfo(subcircuitPanel.selSubId)
                main.openRenameSubDialog(subcircuitPanel.selSubId, info ? info.name : "")
            }
        }
    }

    Rectangle {
        width: parent.width; height: 30; radius: Theme.smallRadius
        visible: !subcircuitPanel.viewOnly
        color: setRootBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
        border.color: Theme.borderStrong; border.width: 1
        Text {
            anchors.centerIn: parent; text: "设为主电路"
            color: Theme.text; font.pixelSize: Theme.fsSmall
        }
        MouseArea {
            id: setRootBtn; anchors.fill: parent
            onClicked: {
                if (!main) return
                main.confirmDialogRef.title = "设为主电路"
                main.confirmDialogRef.message =
                    "确定把该子电路设为主电路？\n当前主电路内容将被移动到原位置。"
                main.confirmDialogRef.action = function() {
                    if (!main) return
                    main.circuitRef.setSubcircuitAsRoot(subcircuitPanel.selSubId)
                    main.selectedSubId = ""
                }
                main.confirmDialogRef.open()
            }
        }
    }

    Rectangle {
        width: parent.width; height: 30; radius: Theme.smallRadius
        color: enterBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
        border.color: Theme.borderStrong; border.width: 1
        Text {
            anchors.centerIn: parent; text: "进入浏览"
            color: Theme.text; font.pixelSize: Theme.fsSmall
        }
        MouseArea {
            id: enterBtn; anchors.fill: parent
            onClicked: {
                if (!main) return
                main.circuitRef.enterSubcircuit(subcircuitPanel.selSubId)
            }
        }
    }

    Rectangle {
        width: parent.width; height: 30; radius: Theme.smallRadius
        visible: !subcircuitPanel.viewOnly
        color: delSubBtn.pressed ? "#6e2020" : Theme.bgButtonDanger
        border.color: Theme.borderDanger; border.width: 1
        Text {
            anchors.centerIn: parent; text: "删除子电路"
            color: Theme.textWhite; font.pixelSize: Theme.fsSmall
        }
        MouseArea {
            id: delSubBtn; anchors.fill: parent
            onClicked: {
                if (!main) return
                var sid = subcircuitPanel.selSubId
                main.confirmDialogRef.title = "删除子电路"
                main.confirmDialogRef.message = "确定删除该子电路及其所有实例？"
                main.confirmDialogRef.action = function() {
                    if (!main) return
                    main.circuitRef.deleteSubcircuit(sid)
                    main.selectedSubId = ""
                }
                main.confirmDialogRef.open()
            }
        }
    }

    Rectangle { width: parent.width; height: 1; color: Theme.border }
}