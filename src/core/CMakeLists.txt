add_library(logicsim_core STATIC
    CircuitContext.h  CircuitContext.cpp
    Component.h
    Wire.h
    IdAllocator.h     IdAllocator.cpp
    UndoData.h
    ValueCodec.h      ValueCodec.cpp
)
target_include_directories(logicsim_core PUBLIC ${LOGICSIM_INCLUDE_ROOT})
target_link_libraries(logicsim_core PUBLIC Qt6::Core)
