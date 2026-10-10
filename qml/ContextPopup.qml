import QtQuick
import QtQuick.Controls
import LogicSim

Popup {
    id: ctxPopup
    property QtObject main: null
    property Item anchorItem: null

    x: anchorItem ? Math.max(10, anchorItem.mapToItem(null, 0, 0).x + anchorItem.width - 240) : 10
    y: anchorItem ? anchorItem.mapToItem(null, 0, anchorItem.height).y : 50
    width: 240
    height: Math.min(320, 36 * Math.max(1, main.editCtxList.length) + 16)
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
        Repeater {
            model: main.editCtxList
            delegate: Rectangle {
                width: ctxPopup.width; height: 36
                color: (main.circuitRef.contextId === modelData.id && !main.circuitViewOnly)
                ? Theme.bgButtonHover
                : (ma.pressed ? Theme.bgButtonHover : "transparent")

                Text {
                    anchors.left: parent.left; anchors.leftMargin: 12
                    anchors.right: parent.right; anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.name + (modelData.id === "" ? "" : "  (子电路)")
                    color: (main.circuitRef.contextId === modelData.id && !main.circuitViewOnly)
                    ? Theme.textWhite : Theme.text
                    font.pixelSize: Theme.fsNormal
                    elide: Text.ElideRight
                }
                MouseArea {
                    id: ma; anchors.fill: parent
                    onClicked: {
                        main.circuitRef.switchEditContext(modelData.id)
                        ctxPopup.close()
                    }
                }
            }
        }
    }
}