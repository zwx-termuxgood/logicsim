cmake_minimum_required(VERSION 3.16)
project(LogicSimPerf LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

# 独立配置入口：只依赖 Qt Core 与被测库，不构建主应用的 QML 界面和 Qt Test 测试。
# 使用同一份 benchmark.cpp，按 LOGICSIM_SOURCE_DIR 选定源码树自身的库源文件清单。
# 比较不同源码树时分别使用构建目录，并保持编译配置一致，减少环境差异对耗时的干扰。
# 默认测量当前项目；也可通过 CMake 缓存变量指定另一源码树，无须为主应用注册基准目标。
get_filename_component(DEFAULT_LOGICSIM_SOURCE_DIR "../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(LOGICSIM_SOURCE_DIR "${DEFAULT_LOGICSIM_SOURCE_DIR}" CACHE PATH "Source tree under measurement")
set(LOGICSIM_INCLUDE_ROOT "${LOGICSIM_SOURCE_DIR}/src")
add_subdirectory("${LOGICSIM_SOURCE_DIR}/src/core" "${CMAKE_CURRENT_BINARY_DIR}/core")
add_subdirectory("${LOGICSIM_SOURCE_DIR}/src/logic" "${CMAKE_CURRENT_BINARY_DIR}/logic")
add_subdirectory("${LOGICSIM_SOURCE_DIR}/src/engine" "${CMAKE_CURRENT_BINARY_DIR}/engine")
add_executable(logicsim_perf benchmark.cpp)
target_link_libraries(logicsim_perf PRIVATE logicsim_engine Qt6::Core)
