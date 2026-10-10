import QtQuick
import QtQuick.Controls
import LogicSim

Column {
    id: splitterProps
    property QtObject main: null

    property alias splitterSplitsInput: splitterSplitsInput

    visible: main.selectedComp &&
             (main.selectedComp.type === "splitter" ||
              main.selectedComp.type === "hub")
    spacing: 4

    Text {
        text: "分段（逗号分隔，和 = 位宽）"
        color: Theme.textGray; font.pixelSize: Theme.fsSmall
    }
    Rectangle {
        width: parent.width; height: 26
        color: Theme.bgInput
        border.color: Theme.borderStrong
        border.width: 1; radius: Theme.smallRadius
        TextField {
            id: splitterSplitsInput
            anchors.fill: parent
            anchors.leftMargin: 5; anchors.rightMargin: 5
            color: Theme.text
            font.pixelSize: Theme.fsSmall; font.family: "monospace"
            background: null; selectByMouse: true
            readOnly: main.circuitViewOnly
            onEditingFinished: {
                if (main.selectedCompId === "" || main.circuitViewOnly) return
                main.circuitRef.setSplitterSplits(main.selectedCompId, text)
            }
        }
    }
    Text {
        text: main.selectedComp && main.selectedComp.type === "splitter"
              ? "分线器：1 输入 → N 输出"
              : "集线器：N 输入 → 1 输出"
        color: Theme.textFaint
        font.pixelSize: Theme.fsTiny
        width: parent.width; wrapMode: Text.Wrap
    }
}
