cmake_minimum_required(VERSION 3.16)
project(LogicSimPerf LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

# Compile identical benchmark code with the selected tree's own source lists.
# No changes or benchmark target registration in the application are required.
get_filename_component(DEFAULT_LOGICSIM_SOURCE_DIR "../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(LOGICSIM_SOURCE_DIR "${DEFAULT_LOGICSIM_SOURCE_DIR}" CACHE PATH "Source tree under measurement")
set(LOGICSIM_INCLUDE_ROOT "${LOGICSIM_SOURCE_DIR}/src")
add_subdirectory("${LOGICSIM_SOURCE_DIR}/src/core" "${CMAKE_CURRENT_BINARY_DIR}/core")
add_subdirectory("${LOGICSIM_SOURCE_DIR}/src/logic" "${CMAKE_CURRENT_BINARY_DIR}/logic")
add_subdirectory("${LOGICSIM_SOURCE_DIR}/src/engine" "${CMAKE_CURRENT_BINARY_DIR}/engine")
add_executable(logicsim_perf benchmark.cpp)
target_link_libraries(logicsim_perf PRIVATE logicsim_engine Qt6::Core)
