set(TEST_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})

function(logicsim_add_test path)
    cmake_parse_arguments(T "" "" "LIBS" ${ARGN})
    # target 名不能含 /，core/test_value_codec → core_test_value_codec
    string(REPLACE "/" "_" target_name "${path}")
    qt_add_executable(${target_name} ${path}.cpp)
    target_include_directories(${target_name} PRIVATE ${TEST_INCLUDE})
    target_link_libraries(${target_name} PRIVATE Qt6::Test ${T_LIBS})
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

logicsim_add_test(core/test_value_codec          LIBS logicsim_core)
logicsim_add_test(core/test_circuit_context      LIBS logicsim_core)
logicsim_add_test(core/test_id_allocator         LIBS logicsim_core)

logicsim_add_test(logic/test_component_logic     LIBS logicsim_logic)
logicsim_add_test(logic/test_wire_network_solver LIBS logicsim_logic)

logicsim_add_test(io/test_json_codec             LIBS logicsim_io)
logicsim_add_test(io/test_clipboard_codec        LIBS logicsim_io)

logicsim_add_test(engine/test_iter_engine        LIBS logicsim_engine)
logicsim_add_test(engine/test_event_engine       LIBS logicsim_engine)

logicsim_add_test(move/test_connector            LIBS logicsim_move)

logicsim_add_test(model/test_circuit_component   LIBS logicsim_model)
logicsim_add_test(model/test_circuit_undo        LIBS logicsim_model)
logicsim_add_test(model/test_circuit_document    LIBS logicsim_model)
logicsim_add_test(model/test_circuit_wire      LIBS logicsim_model)
