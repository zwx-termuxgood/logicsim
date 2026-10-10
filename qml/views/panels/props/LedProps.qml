import QtQuick
import LogicSim

Column {
    id: ledProps
    property QtObject main: null

    visible: main.selectedComp && main.selectedComp.type === "led"
    spacing: 4

    readonly property var ledColorChoices: [
        { name: "黄", value: "#ffd700" },
        { name: "红", value: "#e05252" },
        { name: "绿", value: "#4caf50" },
        { name: "蓝", value: "#58a6ff" },
        { name: "橙", value: "#ffb74d" },
        { name: "紫", value: "#a371f7" },
        { name: "白", value: "#ffffff" }
    ]

    Text {
        text: "LED 颜色"
        color: Theme.textGray
        font.pixelSize: Theme.fsSmall
    }

    Row {
        spacing: 6
        Repeater {
            model: ledProps.ledColorChoices
            delegate: Rectangle {
                width: 24; height: 24; radius: 4
                color: modelData.value
                border.width: {
                    if (!main.selectedComp) return 1
                    var cur = main.selectedComp.color || "#ffd700"
                    return (cur === modelData.value) ? 2 : 1
                }
                border.color: {
                    if (!main.selectedComp) return "#555555"
                    var cur = main.selectedComp.color || "#ffd700"
                    return (cur === modelData.value) ? "#ffffff" : "#555555"
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: !main.circuitViewOnly
                    onClicked: {
                        if (main.circuitViewOnly) return
                        if (main.selectedCompId === "") return
                        main.circuitRef.setComponentProp(
                            main.selectedCompId, "color", modelData.value)
                    }
                }
            }
        }
    }
}
