pragma Singleton
import QtQuick
import "DefinitionManifest.js" as Manifest

QtObject {
    id: registry
    readonly property var definitions: loadDefinitions()
    readonly property var libraryGroups: buildGroups()

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
    function get(type) { return definitions[type] || null }
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
