project(ipc_tests)

add_executable(ipc_stress_client
    ipc_stress_client.cpp
)

target_include_directories(ipc_stress_client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ipc_stress_client
    ipc_core
)

set_target_properties(ipc_stress_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
