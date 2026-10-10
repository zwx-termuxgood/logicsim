import QtQuick
import QtQuick.Controls
import LogicSim

Column {
    id: textProps
    property QtObject main: null

    property alias textContentField: textContentField

    visible: main.selectedComp && main.selectedComp.type === "text"
    spacing: 4

    Text {
        text: "文字内容"; color: Theme.textGray
        font.pixelSize: Theme.fsSmall
    }
    Rectangle {
        width: parent.width; height: 26
        color: Theme.bgInput
        border.color: Theme.borderStrong
        border.width: 1; radius: Theme.smallRadius
        TextField {
            id: textContentField
            anchors.fill: parent
            anchors.leftMargin: 5; anchors.rightMargin: 5
            color: Theme.text
            font.pixelSize: Theme.fsSmall
            background: null; selectByMouse: true
            readOnly: main.circuitViewOnly
            onEditingFinished: {
                if (main.selectedCompId === "" || main.circuitViewOnly) return
                main.circuitRef.setTextContent(main.selectedCompId, text)
            }
        }
    }
}
