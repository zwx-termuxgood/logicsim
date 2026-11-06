add_library(logicsim_logic STATIC
    ComponentLogic.h      ComponentLogic.cpp
    WireNetworkSolver.h   WireNetworkSolver.cpp
    SimEngine.h
)
target_link_libraries(logicsim_logic PUBLIC logicsim_core)
