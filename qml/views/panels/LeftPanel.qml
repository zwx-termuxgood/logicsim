import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Rectangle {
    id: leftPanel
    property QtObject main: null

    // 对外转发 PropertyPanel 的 3 个 alias（Main.qml 通过 leftPanel.xxx 访问）
    property alias inputValueField: propertyPanel.inputValueField
    property alias splitterSplitsInput: propertyPanel.splitterSplitsInput
    property alias textContentField: propertyPanel.textContentField

    anchors.left: parent.left
    anchors.bottom: parent.bottom
    width: Theme.leftPanelW
    color: Theme.bgPanel
    border.color: Theme.border
    border.width: 1

    function findSubInfo(subId) {
        for (var i = 0; i < main.subList.length; ++i) {
            if (main.subList[i].id === subId) return main.subList[i]
        }
        return null
    }

    readonly property var elementGroups: [
        { name: "输入输出", items: [
            { type: "input", name: "输入开关" },
            { type: "const", name: "常量" },
            { type: "output", name: "输出端口" },
            { type: "led", name: "LED" }
        ]},
        { name: "基本逻辑门", items: [
            { type: "and", name: "与门" },
            { type: "or", name: "或门" },
            { type: "not", name: "非门" }
        ]},
        { name: "复合逻辑门", items: [
            { type: "nand", name: "与非" },
            { type: "nor", name: "或非" },
            { type: "xor", name: "异或" },
            { type: "xnor", name: "同或" }
        ]},
        { name: "传输门 / 晶体管", items: [
            { type: "tgate", name: "传输门" },
            { type: "ntran", name: "NMOS 管" },
            { type: "ptran", name: "PMOS 管" }
        ]},
        { name: "工具", items: [
            { type: "splitter", name: "分线器" },
            { type: "hub", name: "集线器" }
        ]},
        { name: "控制", items: [
            { type: "clock", name: "时钟" },
            { type: "text", name: "文字" }
        ]}
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: Theme.bgBar
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                width: 60; height: 20
                radius: Theme.smallRadius
                color: leftCollapseMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                border.color: Theme.borderStrong; border.width: 1
                Text {
                    anchors.centerIn: parent; text: "◀ 收起"
                    color: Theme.text; font.pixelSize: Theme.fsTiny
                }
                MouseArea {
                    id: leftCollapseMa
                    anchors.fill: parent
                    onClicked: main.leftCollapsed = true
                }
            }
        }

        Flickable {
            id: paletteFlick
            Layout.fillWidth: true
            Layout.preferredHeight: main.mode !== "control" && !main.circuitViewOnly
                                  ? Math.round(main.windowHeight * 0.5) : 0
            visible: main.mode !== "control" && !main.circuitViewOnly
            clip: true
            contentWidth: width
            contentHeight: paletteColumn.height + 12
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: paletteColumn
                width: paletteFlick.width
                spacing: 3
                Item { width: 1; height: 4 }

                Repeater {
                    model: leftPanel.elementGroups
                    delegate: Column {
                        width: paletteColumn.width
                        spacing: 2
                        Rectangle {
                            width: parent.width; height: 22; color: Theme.bgDialog
                            Text {
                                anchors.left: parent.left; anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name
                                color: Theme.textWeak
                                font.pixelSize: Theme.fsSmall; font.bold: true
                            }
                        }
                        Repeater {
                            model: modelData.items
                            delegate: Rectangle {
                                width: parent.width - 10; x: 5; height: 28
                                radius: Theme.smallRadius
                                color: (main.placingType === modelData.type && main.placingSubId === "")
                                       ? Theme.bgButtonHover
                                       : (itemMa.pressed ? Theme.bgButtonHover : Theme.bgButton)
                                border.color: (main.placingType === modelData.type && main.placingSubId === "")
                                              ? Theme.borderBtnActive : Theme.borderWeak
                                border.width: 1
                                Text {
                                    anchors.left: parent.left; anchors.leftMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.name
                                    color: Theme.text; font.pixelSize: Theme.fsNormal
                                }
                                MouseArea {
                                    id: itemMa; anchors.fill: parent
                                    onClicked: {
                                        if (main.placingType === modelData.type && main.placingSubId === "") {
                                            main.placingType = ""
                                        } else {
                                            main.placingType = modelData.type
                                            main.placingSubId = ""
                                            main.statusText = "已选择 " + modelData.name + "，点画布放置"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width; height: 22; color: Theme.bgDialog
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: "子电路 (" + main.subList.length + ")"
                        color: Theme.accentPurple
                        font.pixelSize: Theme.fsSmall; font.bold: true
                    }
                }
                Rectangle {
                    width: parent.width - 10; x: 5; height: 28; radius: Theme.smallRadius
                    color: newSubMa.pressed ? Theme.bgButtonHover : Theme.bgButton
                    border.color: Theme.borderSub; border.width: 1
                    Text {
                        anchors.centerIn: parent; text: "+ 新建子电路"
                        color: Theme.accentPurple; font.pixelSize: Theme.fsSmall
                    }
                    MouseArea { id: newSubMa; anchors.fill: parent
                        onClicked: main.subNameDialogRef.open() }
                }

                Repeater {
                    model: main.subList
                    delegate: Rectangle {
                        width: parent.width - 10; x: 5; height: 32
                        radius: Theme.smallRadius
                        color: (main.placingType === "sub" && main.placingSubId === modelData.id)
                               ? Theme.bgButtonHover
                               : (subItemMa.pressed ? Theme.bgButtonHover : Theme.bgButton)
                        border.color: (main.placingType === "sub" && main.placingSubId === modelData.id)
                                      ? Theme.accentPurple : Theme.borderWeak
                        border.width: 1

                        Text {
                            anchors.left: parent.left; anchors.leftMargin: 10
                            anchors.right: moreBtn.left; anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.name + " [" + modelData.inCount + "/" + modelData.outCount + "]"
                            color: Theme.text; font.pixelSize: Theme.fsSmall
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            id: subItemMa
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.right: moreBtn.left
                            acceptedButtons: Qt.LeftButton | Qt.RightButton

                            onPressAndHold: {
                                subCtxMenu.subId = modelData.id
                                subCtxMenu.subName = modelData.name
                                subCtxMenu.popup()
                            }
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.LeftButton) {
                                    main.selectedSubId = modelData.id
                                    main.placingType = "sub"
                                    main.placingSubId = modelData.id
                                    main.statusText = "已选择 " + modelData.name + "，点画布放置"
                                } else if (mouse.button === Qt.RightButton) {
                                    subCtxMenu.subId = modelData.id
                                    subCtxMenu.subName = modelData.name
                                    subCtxMenu.popup()
                                }
                            }
                        }

                        Rectangle {
                            id: moreBtn
                            width: 28; height: parent.height
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            color: moreMa.pressed ? Theme.bgButtonHover : "transparent"
                            radius: Theme.smallRadius
                            Text {
                                anchors.centerIn: parent
                                text: "⋯"
                                color: Theme.textGray
                                font.pixelSize: 16
                            }
                            MouseArea {
                                id: moreMa; anchors.fill: parent
                                onClicked: {
                                    subCtxMenu.subId = modelData.id
                                    subCtxMenu.subName = modelData.name
                                    subCtxMenu.popup()
                                }
                            }
                        }
                    }
                }
                Item { width: 1; height: 8 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        PropertyPanel {
            id: propertyPanel
            main: leftPanel.main
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    // ---- 子电路右键菜单 ----
    Menu {
        id: subCtxMenu
        property string subId: ""
        property string subName: ""

        MenuItem {
            text: "选中（显示属性）"
            onTriggered: {
                main.selectedSubId = subCtxMenu.subId
                main.placingType = "sub"
                main.placingSubId = subCtxMenu.subId
            }
        }
        MenuItem {
            text: "重命名"
            onTriggered: {
                renameDialog.subId = subCtxMenu.subId
                renameInput.text = subCtxMenu.subName
                renameDialog.open()
            }
        }
        MenuItem {
            text: "设为主电路"
            onTriggered: main.circuitRef.setSubcircuitAsRoot(subCtxMenu.subId)
        }
        MenuSeparator { }
        MenuItem {
            text: "删除"
            onTriggered: {
                main.confirmDialogRef.title = "删除子电路"
                main.confirmDialogRef.message = "确定删除该子电路及其所有实例？"
                main.confirmDialogRef.action = function() {
                    main.circuitRef.deleteSubcircuit(subCtxMenu.subId)
                    main.selectedSubId = ""
                }
                main.confirmDialogRef.open()
            }
        }
    }

    function openRenameSubDialog(subId, name) {
        renameDialog.subId = subId
        renameInput.text = name
        renameDialog.open()
    }

    Dialog {
        id: renameDialog
        property string subId: ""

        anchors.centerIn: parent
        modal: true
        title: "重命名子电路"
        standardButtons: Dialog.NoButton
        width: 300
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
                text: "输入新名称"
                color: Theme.textStrong
                font.pixelSize: Theme.fsMedium
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                color: Theme.bgInput
                border.color: Theme.borderDialog
                border.width: 1
                radius: Theme.smallRadius
                TextField {
                    id: renameInput
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    color: Theme.text
                    font.pixelSize: Theme.fsNormal
                    background: null
                    selectByMouse: true
                    onAccepted: renameDialog.doRename()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Rectangle {
                    Layout.preferredWidth: 70; Layout.preferredHeight: 30
                    radius: Theme.buttonRadius
                    color: cc1r.pressed ? Theme.bgButtonHover : Theme.bgButton
                    border.color: Theme.borderDialog; border.width: 1
                    Text { anchors.centerIn: parent; text: "取消"
                        color: Theme.text; font.pixelSize: Theme.fsNormal }
                    MouseArea { id: cc1r; anchors.fill: parent
                        onClicked: renameDialog.close() }
                }
                Rectangle {
                    Layout.preferredWidth: 80; Layout.preferredHeight: 30
                    radius: Theme.buttonRadius
                    color: cc2r.pressed ? Theme.bgButtonSuccessHi : Theme.bgButtonSuccess
                    border.color: Theme.borderSuccess; border.width: 1
                    Text { anchors.centerIn: parent; text: "确定"
                        color: Theme.textWhite; font.pixelSize: Theme.fsNormal }
                    MouseArea { id: cc2r; anchors.fill: parent
                        onClicked: renameDialog.doRename() }
                }
            }
        }

        function doRename() {
            var name = renameInput.text.trim()
            if (name.length === 0) return
            main.circuitRef.setSubcircuitName(renameDialog.subId, name)
            renameDialog.close()
        }
    }
}
