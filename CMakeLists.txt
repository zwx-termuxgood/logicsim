cmake_minimum_required(VERSION 3.16)
project(LogicSim VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Qml Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.5)

if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

set(LOGICSIM_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_subdirectory(src/core)
add_subdirectory(src/logic)
add_subdirectory(src/io)
add_subdirectory(src/move)
add_subdirectory(src/engine)
add_subdirectory(src/model)

# ── 可执行文件在顶层创建 ────────────────────────────────────
# target 的 source dir = 项目根，Qt Creator 才会把 qml/ 显示在根下
qt_add_executable(LogicSim src/app/main.cpp)

target_link_libraries(LogicSim PRIVATE
    logicsim_model
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
)

if(ANDROID)
    set_property(TARGET LogicSim PROPERTY
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android")
endif()

# ── QML 模块 ────────────────────────────────────────────────
set(QML_FILES
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
    qml/views/dialogs/UnsavedDialog.qml
    qml/views/dialogs/ImportedFilesPopup.qml
)

set(QML_RESOURCES
    qml/views/canvas/Geometry.js
    qml/views/canvas/Painter.js
    qml/views/canvas/interactions/CanvasInteraction.js
)

# 资源别名去掉 "qml/" 前缀，让 loadFromModule("LogicSim", "Main") 找到 Main.qml
foreach(f ${QML_FILES} ${QML_RESOURCES})
    string(REPLACE "qml/" "" alias "${f}")
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS "${alias}")
endforeach()

# Theme.qml 是 singleton
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(LogicSim
    URI LogicSim
    VERSION 1.0
    OUTPUT_DIRECTORY LogicSimQml
    QML_FILES ${QML_FILES}
    RESOURCES ${QML_RESOURCES}
)

enable_testing()
add_subdirectory(tests)
