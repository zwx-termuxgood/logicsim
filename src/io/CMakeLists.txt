add_library(logicsim_io STATIC
    JsonCodec.h       JsonCodec.cpp
    ClipboardCodec.h  ClipboardCodec.cpp
    RecentStore.h     RecentStore.cpp
)
target_link_libraries(logicsim_io PUBLIC logicsim_core Qt6::Core)
