cmake_minimum_required(VERSION 3.16)
project(LogicSim VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Qml)

qt_standard_project_setup(REQUIRES 6.5)

set(CIRCUIT_SOURCES
    circuit/Circuit.cpp
    circuit/core/CircuitContext.cpp
    circuit/core/ValueCodec.cpp
    circuit/core/IdAllocator.cpp
    circuit/logic/ComponentLogic.cpp
    circuit/logic/event/EventEngine.cpp
    circuit/logic/event/EventEngine_build.cpp
    circuit/logic/event/EventEngine_run.cpp
    circuit/logic/event/EventEngine_trigger.cpp
    circuit/logic/iter/IterEngine.cpp
    circuit/io/JsonCodec.cpp
    circuit/io/ClipboardCodec.cpp
    circuit/io/RecentStore.cpp
    circuit/app/Circuit_lifecycle.cpp
    circuit/app/Circuit_component.cpp
    circuit/app/Circuit_wire.cpp
    circuit/app/Circuit_subcircuit.cpp
    circuit/app/Circuit_simulation.cpp
    circuit/app/Circuit_selection.cpp
    circuit/app/Circuit_query.cpp
    circuit/app/Circuit_catalog.cpp
)

set(QML_SOURCES
    qml/Main.qml
    qml/Theme.qml
    qml/views/bars/TopBar.qml
    qml/views/bars/ClockBar.qml
    qml/views/bars/StatusBar.qml
    qml/views/canvas/CanvasView.qml
    qml/views/panels/LeftPanel.qml
    qml/views/panels/PropertyPanel.qml
    qml/views/panels/SubcircuitPanel.qml
    qml/views/panels/props/LedProps.qml
    qml/views/panels/props/SplitterProps.qml
    qml/views/panels/props/TextProps.qml
    qml/views/dialogs/ConfirmDialog.qml
    qml/views/dialogs/CloseConfirmDialog.qml
    qml/views/dialogs/ExitConfirmDialog.qml
    qml/views/dialogs/SubNameDialog.qml
    qml/views/dialogs/SettingsPage.qml
    qml/views/dialogs/ContextPopup.qml
    qml/views/dialogs/MenuPopup.qml
    qml/views/dialogs/RecentPopup.qml
    qml/views/dialogs/StatsPopup.qml
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# ★ target 名从 applogicsim 改成 LogicSim，和 URI 一致
qt_add_executable(LogicSim main.cpp ${CIRCUIT_SOURCES})

qt_add_qml_module(LogicSim
    URI LogicSim
    VERSION 1.0
    OUTPUT_DIRECTORY LogicSimQml       # ★ 关键：QML 元数据换到 LogicSimQml/，避免和可执行文件同名冲突
    QML_FILES ${QML_SOURCES}
    RESOURCES
        qml/views/canvas/Geometry.js
        qml/views/canvas/Painter.js
        qml/views/canvas/interactions/CanvasInteraction.js
        SOURCES circuit/core/UndoData.h
)

target_include_directories(LogicSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/circuit)

target_link_libraries(LogicSim PRIVATE
    Qt6::Quick Qt6::QuickControls2 Qt6::Qml
)

if(ANDROID)
    set_property(TARGET LogicSim PROPERTY
        QT_ANDROID_PACKAGE_SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()
