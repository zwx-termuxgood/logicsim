cmake_minimum_required(VERSION 3.16)
project(LogicSim VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Qml)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(applogicsim main.cpp circuit.cpp circuit.h)

qt_add_qml_module(applogicsim
    URI LogicSim
    VERSION 1.0
    QML_FILES Main.qml
    SOURCES circuit.cpp circuit.h
)

target_link_libraries(applogicsim PRIVATE
    Qt6::Quick Qt6::QuickControls2 Qt6::Qml
)

# Android：把 android/AndroidManifest.xml 打进 APK
if(ANDROID)
    set_property(TARGET applogicsim PROPERTY
        QT_ANDROID_PACKAGE_SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()