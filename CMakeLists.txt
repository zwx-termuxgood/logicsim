cmake_minimum_required(VERSION 3.16)
project(LogicSim VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Qml)

qt_standard_project_setup(REQUIRES 6.5)

set(CIRCUIT_SOURCES
    circuit/UndoData.h
    circuit/CircuitContext.h
    circuit/CircuitContext.cpp
    circuit/ComponentTraits.h
    circuit/ComponentTraits.cpp
    circuit/ValueCodec.h
    circuit/ValueCodec.cpp
    circuit/PortGeometry.h
    circuit/PortGeometry.cpp
    circuit/CircuitEvaluator.h
    circuit/CircuitEvaluator.cpp
    circuit/EventEngine.h
    circuit/EventEngine.cpp
    circuit/circuit.h
    circuit/circuit.cpp
    circuit/circuit_component.cpp
    circuit/circuit_subcircuit.cpp
    circuit/circuit_wire.cpp
    circuit/circuit_ports.cpp
    circuit/circuit_value.cpp
    circuit/circuit_evaluate.cpp
    circuit/circuit_clock.cpp
    circuit/circuit_serialize.cpp
    circuit/circuit_recent.cpp
    circuit/circuit_stats.cpp
    circuit/circuit_clipboard.cpp
)

set(QML_SOURCES
    qml/Main.qml
    qml/Theme.qml
    qml/TopBar.qml
    qml/ClockBar.qml
    qml/LeftPanel.qml
    qml/StatusBar.qml
    qml/CanvasView.qml
    qml/SubNameDialog.qml
    qml/CloseConfirmDialog.qml
    qml/ExitConfirmDialog.qml
    qml/ConfirmDialog.qml
    qml/StatsPopup.qml
    qml/RecentPopup.qml
    qml/ContextPopup.qml
    qml/MenuPopup.qml
    qml/SettingsPage.qml
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(applogicsim main.cpp ${CIRCUIT_SOURCES})

qt_add_qml_module(applogicsim
    URI LogicSim
    VERSION 1.0
    QML_FILES ${QML_SOURCES}
    RESOURCES
        qml/Geometry.js
        qml/CanvasPainter.js
        SOURCES circuit/UndoData.h
)

target_include_directories(applogicsim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/circuit)

target_link_libraries(applogicsim PRIVATE
    Qt6::Quick Qt6::QuickControls2 Qt6::Qml
)

if(ANDROID)
    set_property(TARGET applogicsim PROPERTY
        QT_ANDROID_PACKAGE_SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()