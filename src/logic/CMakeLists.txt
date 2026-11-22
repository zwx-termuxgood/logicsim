# File stems are factory identifiers (e.g. And.cpp -> createAndDefinition).
# CONFIGURE_DEPENDS makes adding a builtin sufficient to regenerate this list.
file(GLOB component_builtins CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../components/builtin/*.cpp")
list(SORT component_builtins)
set(factory_declarations "")
set(factory_calls "")
foreach(source IN LISTS component_builtins)
    get_filename_component(stem "${source}" NAME_WE)
    string(APPEND factory_declarations "std::unique_ptr<Definition> create${stem}Definition();\n")
    string(APPEND factory_calls "    result.push_back(create${stem}Definition());\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/BuiltinFactories.inc"
    "${factory_declarations}\nstatic std::vector<std::unique_ptr<Definition>> builtinDefinitions() {\n    std::vector<std::unique_ptr<Definition>> result;\n${factory_calls}    return result;\n}\n")

add_library(logicsim_logic STATIC
    ComponentLogic.h      ComponentLogic.cpp
    ../components/ComponentDefinition.h ../components/ComponentDefinition.cpp
    ${component_builtins}
    WireNetworkSolver.h   WireNetworkSolver.cpp
    SimEngine.h
)
target_include_directories(logicsim_logic PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(logicsim_logic PUBLIC logicsim_core)
