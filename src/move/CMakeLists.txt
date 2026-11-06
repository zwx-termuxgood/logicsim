add_library(logicsim_move STATIC
    AvoidanceMap.h    AvoidanceMap.cpp
    SearchNode.h      SearchNode.cpp
    ConnectionData.h
    MoveRequest.h
    MoveResult.h
    MoveGesture.h     MoveGesture.cpp
    Connector.h       Connector.cpp
    ConnectorThread.h ConnectorThread.cpp
)
target_link_libraries(logicsim_move PUBLIC logicsim_logic Qt6::Core)
