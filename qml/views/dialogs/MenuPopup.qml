import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LogicSim

Popup {
    id: menuPopup
    property QtObject main: null
    property Item anchorItem: null

    readonly property real screenW: main ? main.width : 360
    readonly property real screenH: main ? main.height : 640

    width: Math.min(260, screenW - 16)
    height: Math.min(340, screenH - 40)

    x: {
        if (!anchorItem) return 8
        var ax = anchorItem.mapToItem(null, 0, 0).x
        var maxX = screenW - width - 8
        return Math.max(8, Math.min(ax, maxX))
    }

    y: {
        if (!anchorItem) return 50
        var ay = anchorItem.mapToItem(null, 0, anchorItem.height).y + 4
        var maxY = screenH - height - 8
        return Math.max(8, Math.min(ay, maxY))
    }

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    background: Rectangle {
        color: Theme.bgDialog
        border.color: Theme.borderDialog
        border.width: 1
        radius: 8
    }

    contentItem: Flickable {
        id: menuFlick
        implicitWidth: menuPopup.width
        implicitHeight: contentColumn.implicitHeight
        contentWidth: width
        contentHeight: contentColumn.height
        clip: true
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn
            width: menuFlick.width
            spacing: 0

            Rectangle {
                width: contentColumn.width
                height: 40
                color: Theme.bgDialogHeader
                radius: 8
                Text {
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "菜单"
                    color: Theme.textGray
                    font.pixelSize: Theme.fsMedium
                    font.bold: true
                }
            }

            MenuRow {
                label: "保存"
                onTriggered: { menuPopup.close(); main.doSave() }
            }
            MenuRow {
                label: "另存为"
                onTriggered: { menuPopup.close(); main.doSaveAs() }
            }
            MenuRow {
                label: "打开"
                onTriggered: { menuPopup.close(); main.openDialogRef.open() }
            }
            MenuRow {
                label: "最近"
                onTriggered: { menuPopup.close(); main.recentPopupRef.open() }
            }
            MenuRow {
                label: "统计"
                onTriggered: { menuPopup.close(); main.statPopupRef.open() }
            }
            MenuRow {
                label: "新建"
                onTriggered: {
                    menuPopup.close()
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

            Rectangle {
                width: contentColumn.width
                height: 1
                color: Theme.border
            }

            MenuRow {
                label: "⚙ 设置"
                onTriggered: { menuPopup.close(); main.settingsOpen = true }
            }

            Item { width: 1; height: 8 }
        }
    }

    component MenuRow: Rectangle {
        property string label: ""
        signal triggered()

        width: contentColumn.width
        height: 40
        color: ma.pressed ? Theme.bgButtonHover : "transparent"
        radius: 0

        Text {
            anchors.left: parent.left; anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: parent.label
            color: Theme.text
            font.pixelSize: Theme.fsNormal
        }
        MouseArea {
            id: ma
            anchors.fill: parent
            onClicked: parent.triggered()
        }
    }
}
