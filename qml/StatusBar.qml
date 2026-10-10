import QtQuick
import LogicSim

Rectangle {
    id: statusBar
    property QtObject main: null

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    height: Theme.statusBarH
    color: Theme.bg

    Text {
        anchors.left: parent.left; anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: "[" + main.mode + "] " + main.statusText
        color: main.errorList.length > 0 ? Theme.accentRed : Theme.textWeak
        font.pixelSize: Theme.fsSmall
        elide: Text.ElideRight
    }
}