add_library(logicsim_engine STATIC
    event/EventEngine.h
    event/EventEngine.cpp
    event/EventEngine_build.cpp
    event/EventEngine_run.cpp
    event/EventEngine_trigger.cpp
    iter/IterEngine.h
    iter/IterEngine.cpp
)
target_link_libraries(logicsim_engine PUBLIC logicsim_logic Qt6::Core)
