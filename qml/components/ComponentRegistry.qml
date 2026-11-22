pragma Singleton
import QtQuick
import "DefinitionManifest.js" as Manifest

// 界面定义注册中心：单例首次使用时加载清单，供元件库、属性面板和画布共用。
// 定义对象以 registry 为父对象长期保留，不按画布元件数量重复创建。
QtObject {
    id: registry
    readonly property var definitions: loadDefinitions()
    readonly property var libraryGroups: buildGroups()

    // 同步加载并校验类型唯一性；失败立即抛错，避免缺失定义被静默当成未知元件。
    // 临时 Component 工厂创建完成即销毁，生成的定义对象仍由 registry 持有。
    function loadDefinitions() {
        var result = Object.create(null)
        for (var i = 0; i < Manifest.files.length; ++i) {
            var source = Qt.createComponent(Qt.resolvedUrl(Manifest.files[i]), Component.PreferSynchronous)
            if (source.status !== Component.Ready)
                throw new Error("Component definition: " + source.errorString())
            var definition = source.createObject(registry)
            if (!definition || !definition.typeId || result[definition.typeId])
                throw new Error("Missing or duplicate component type: " + Manifest.files[i])
            result[definition.typeId] = definition
            source.destroy()
        }
        return result
    }
    // 按类型返回共享定义；未知类型返回 null，调用方自行采用通用外观。
    function get(type) { return definitions[type] || null }
    // 仅收集非空分类，按分类序号、库内序号、类型名稳定排序，返回面板使用的分组数组。
    function buildGroups() {
        var entries = []
        for (var type in definitions) {
            var d = definitions[type]
            if (d.category.length) entries.push(d)
        }
        entries.sort(function(a, b) {
            return a.categoryOrder - b.categoryOrder || a.libraryOrder - b.libraryOrder
                || a.typeId.localeCompare(b.typeId)
        })
        var groups = []
        for (var i = 0; i < entries.length; ++i) {
            var entry = entries[i]
            var group = null
            for (var j = 0; j < groups.length; ++j)
                if (groups[j].name === entry.category) group = groups[j]
            if (!group) { group = { name: entry.category, items: [] }; groups.push(group) }
            group.items.push({ type: entry.typeId, name: entry.displayName })
        }
        return groups
    }
    // 启动时双向核对模型与界面类型集合；接口缺失返回 false，不一致抛错，一致返回 true。
    // 这里只校验类型注册对齐，不检查端口数量、信号语义或文件兼容性。
    function validate(circuit) {
        if (!circuit.registeredComponentTypes) {
            console.warn("Component parity validation unavailable: Circuit.registeredComponentTypes() is missing")
            return false
        }
        var types = circuit.registeredComponentTypes()
        var missing = []
        for (var i = 0; i < types.length; ++i)
            if (!get(types[i])) missing.push("QML: " + types[i])
        for (var type in definitions)
            if (types.indexOf(type) < 0) missing.push("C++: " + type)
        if (missing.length) throw new Error("Component registration mismatch: " + missing.join(", "))
        return true
    }
}
