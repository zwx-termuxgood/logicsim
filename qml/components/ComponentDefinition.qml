import QtQuick
import "../views/panels/props"
import "../views/canvas/Geometry.js" as Geometry

QtObject {
    property string typeId: ""
    property string displayName: ""
    property string category: ""
    property int categoryOrder: 0
    property int libraryOrder: 0
    property bool multiInput: false
    property bool gateDelay: false
    property bool inputLike: false
    property bool displayBase: false
    property bool rotatable: true
    property bool showName: true
    property real labelYOffset: 0
    property string portDescription: ""
    property Component propertyEditor: null
    property string editorField: ""
    function sizeSignature(comp) { return "" }
    function size(comp) { return { w: 80, h: Math.max(60, (comp.inputCount || 2) * 24) } }
    function label(comp, circuit) { return "" }
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) { return false }
    function paintInputLabel(ctx, k, psp, viewScale) {}
    function paintOutputLabel(ctx, k2, psp2, viewScale) {}
    function paintPortNames(ctx, comp, ins, outs, viewScale, viewX, viewY) {}
}
