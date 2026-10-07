import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Rectangle {
    id: topBar
    property QtObject main: null

    property alias ctxBtn: ctxBtn
    property alias recentBtn: recentBtn
    property alias statBtn: statBtn

    anchors.top: parent.top
    anchors.left: parent.left
    anchors.right: parent.right
    height: Theme.topBarH
    color: Theme.bgPanel
    border.color: Theme.border
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 6; anchors.rightMargin: 6
        anchors.topMargin: 5; anchors.bottomMargin: 5
        spacing: 3

        Text {
            text: "LogicSim"; color: Theme.textStrong
            font.pixelSize: Theme.fsLarge; font.bold: true
        }
        Rectangle { width: 1; height: 22; color: Theme.borderStrong }

        Repeater {
            model: [ { id: "edit", name: "编辑" },
                     { id: "control", name: "控制" },
                     { id: "select", name: "选择" } ]
            delegate: Rectangle {
                Layout.preferredWidth: 44; Layout.preferredHeight: 28
                radius: Theme.buttonRadius
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

        Rectangle { width: 1; height: 22; color: Theme.borderStrong }

        // ============ 撤销 / 重做 ============
        Rectangle {
            Layout.preferredWidth: 28; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
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
            Layout.preferredWidth: 28; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
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

        Rectangle { width: 1; height: 22; color: Theme.borderStrong }

        Rectangle {
            Layout.preferredWidth: 42; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
            color: saveBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
            border.color: Theme.borderStrong; border.width: 1
            Text { anchors.centerIn: parent; text: "保存"
                color: Theme.text; font.pixelSize: Theme.fsSmall }
            MouseArea { id: saveBtn; anchors.fill: parent
                onClicked: main.doSave() }
        }
        Rectangle {
            Layout.preferredWidth: 54; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
            color: saveAsBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
            border.color: Theme.borderStrong; border.width: 1
            Text { anchors.centerIn: parent; text: "另存为"
                color: Theme.text; font.pixelSize: Theme.fsSmall }
            MouseArea { id: saveAsBtn; anchors.fill: parent
                onClicked: main.doSaveAs() }
        }
        Rectangle {
            Layout.preferredWidth: 42; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
            color: openBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
            border.color: Theme.borderStrong; border.width: 1
            Text { anchors.centerIn: parent; text: "打开"
                color: Theme.text; font.pixelSize: Theme.fsSmall }
            MouseArea { id: openBtn; anchors.fill: parent
                onClicked: main.openDialogRef.open() }
        }
        Rectangle {
            Layout.preferredWidth: 42; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
            color: recentBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
            border.color: Theme.borderStrong; border.width: 1
            Text { anchors.centerIn: parent; text: "最近"
                color: Theme.text; font.pixelSize: Theme.fsSmall }
            MouseArea { id: recentBtn; anchors.fill: parent
                onClicked: main.recentPopupRef.open() }
        }
        Rectangle {
            Layout.preferredWidth: 42; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
            color: statBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
            border.color: Theme.borderStrong; border.width: 1
            Text { anchors.centerIn: parent; text: "统计"
                color: Theme.text; font.pixelSize: Theme.fsSmall }
            MouseArea { id: statBtn; anchors.fill: parent
                onClicked: main.statPopupRef.open() }
        }
        Rectangle {
            Layout.preferredWidth: 42; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
            color: newBtn.pressed ? Theme.bgButtonHover : Theme.bgButton
            border.color: Theme.borderStrong; border.width: 1
            Text { anchors.centerIn: parent; text: "新建"
                color: Theme.text; font.pixelSize: Theme.fsSmall }
            MouseArea {
                id: newBtn; anchors.fill: parent
                onClicked: {
                    main.confirmDialogRef.title = "新建电路"
                    main.confirmDialogRef.message = "确定清空并新建？"
                    main.confirmDialogRef.action = function() {
                        main.circuitRef.clear()
                        main.currentFilePath = ""
                        main.dirty = false
                    }
                    main.confirmDialogRef.open()
                }
            }
        }

        Rectangle { width: 1; height: 22; color: Theme.borderStrong }

        Rectangle {
            Layout.preferredWidth: 28; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
            color: zo.pressed ? Theme.bgButtonHover : Theme.bgButton
            border.color: Theme.borderStrong; border.width: 1
            Text { anchors.centerIn: parent; text: "−"
                color: Theme.text; font.pixelSize: 15 }
            MouseArea { id: zo; anchors.fill: parent
                onClicked: main.zoomOut() }
        }
        Rectangle {
            Layout.preferredWidth: 46; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
            color: rst.pressed ? Theme.bgButtonHover : Theme.bgButton
            border.color: Theme.borderStrong; border.width: 1
            Text { anchors.centerIn: parent
                text: Math.round(main.viewScale * 100) + "%"
                color: Theme.text; font.pixelSize: Theme.fsTiny }
            MouseArea { id: rst; anchors.fill: parent
                onClicked: main.resetView() }
        }
        Rectangle {
            Layout.preferredWidth: 28; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
            color: zi.pressed ? Theme.bgButtonHover : Theme.bgButton
            border.color: Theme.borderStrong; border.width: 1
            Text { anchors.centerIn: parent; text: "+"
                color: Theme.text; font.pixelSize: 15 }
            MouseArea { id: zi; anchors.fill: parent
                onClicked: main.zoomIn() }
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            id: ctxBtn
            Layout.minimumWidth: 120; Layout.preferredWidth: 150
            Layout.maximumWidth: 180; Layout.preferredHeight: 28
            radius: Theme.buttonRadius
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
            text: "●"; color: Theme.accentDirty; font.pixelSize: 14
        }
    }
}