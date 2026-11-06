add_library(logicsim_model STATIC
    Circuit.h  Circuit.cpp
    impl/Circuit_lifecycle.cpp
    impl/Circuit_component.cpp
    impl/Circuit_wire.cpp
    impl/Circuit_subcircuit.cpp
    impl/Circuit_simulation.cpp
    impl/Circuit_selection.cpp
    impl/Circuit_query.cpp
    impl/Circuit_catalog.cpp
)
target_link_libraries(logicsim_model PUBLIC
    logicsim_core
    logicsim_logic
    logicsim_io
    logicsim_move
    logicsim_engine
    Qt6::Core
)
