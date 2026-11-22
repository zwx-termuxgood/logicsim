import QtQuick
import "../views/panels/props"
import "../views/canvas/Geometry.js" as Geometry

// 元件类型的界面契约：注册表为每个类型创建一个共享定义，随注册表存活。
// 定义保存分类、编辑能力与绘制方法；每个电路实例的数据由 comp 快照传入。
QtObject {
    // 稳定类型标识与库中名称；分类为空的类型可注册，但不出现在普通元件库。
    property string typeId: ""
    property string displayName: ""
    property string category: ""
    property int categoryOrder: 0
    property int libraryOrder: 0
    // 能力开关供属性面板选择通用编辑项；不在此实现门逻辑或改变模型端口。
    property bool multiInput: false
    property bool gateDelay: false
    property bool inputLike: false
    property bool displayBase: false
    // 旋转与名称控制通用界面；labelYOffset 是类型内部标签的世界坐标纵向偏移。
    property bool rotatable: true
    property bool showName: true
    property real labelYOffset: 0
    property string portDescription: ""
    // propertyEditor 是编辑器工厂，由属性面板 Loader 按需实例化，不是每种类型常驻的控件。
    // editorField 指定需要外部同步的文本框；空串表示无需该文本框同步入口。
    property Component propertyEditor: null
    property string editorField: ""
    // 返回类型额外的尺寸依赖签名；默认无额外依赖，公共签名由 CanvasView 维护。
    function sizeSignature(comp) { return "" }
    // 返回旋转前的世界尺寸 {w,h}；画布负责旋转时交换宽高及屏幕缩放。
    function size(comp) { return { w: 80, h: Math.max(60, (comp.inputCount || 2) * 24) } }
    // 返回类型内部的标签文本；circuit 可用于查询模型名称，空串表示无内部标签。
    function label(comp, circuit) { return "" }
    // ctx 是已应用外壳旋转的二维上下文；sp 为绘制框左上角，w/h 为屏幕尺寸。
    // viewScale 用于缩放细节，isSel 为单选态，simplify 要求省略细节，circuit 提供只读查询。
    // 返回 true 结束本元件绘制；false 允许继续通用端口与名称处理，简化模式仍会提前结束。
    function paint(ctx, comp, sp, w, h, viewScale, isSel, simplify, circuit) { return false }
    // 端口标注回调无返回值；索引从 0 开始，psp/psp2 是撤销外壳旋转后的屏幕端口坐标。
    function paintInputLabel(ctx, k, psp, viewScale) {}
    function paintOutputLabel(ctx, k2, psp2, viewScale) {}
    // 整体接口名回调无返回值；ins/outs 为模型端口相对偏移，需自行换算到屏幕坐标。
    function paintPortNames(ctx, comp, ins, outs, viewScale, viewX, viewY) {}
}
