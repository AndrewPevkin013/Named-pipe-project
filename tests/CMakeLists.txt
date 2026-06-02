# project(ipc_tests)

# add_executable(ipc_stress_client
#     ipc_stress_client.cpp
# )

# target_include_directories(ipc_stress_client PRIVATE
#     ${CMAKE_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}
# )

# target_link_libraries(ipc_stress_client
#     ipc_core
# )

# set_target_properties(ipc_stress_client PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
# )
project(ipc_tests)

add_executable(ipc_multiclient_test
    ipc_multiclient_test.cpp
)

target_include_directories(ipc_multiclient_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ipc_multiclient_test
    ipc_core
)

add_executable(ipc_large_message_benchmark
    ipc_large_message_benchmark.cpp
)

target_include_directories(ipc_large_message_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ipc_large_message_benchmark
    ipc_core
)

add_executable(ipc_small_message_benchmark
    ipc_small_message_benchmark.cpp
)

target_include_directories(ipc_small_message_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ipc_small_message_benchmark
    ipc_core
)

find_package(Boost REQUIRED)

add_executable(boost_mq_benchmark
    boost_mq_benchmark.cpp
)

target_include_directories(boost_mq_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    D:/local/vcpkg/installed/x64-windows/include
)

add_executable(boost_mq_large_message_benchmark
    boost_mq_large_message_benchmark.cpp
)

target_include_directories(boost_mq_large_message_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    D:/local/vcpkg/installed/x64-windows/include
)

find_package(cppzmq CONFIG REQUIRED)

add_executable(zmq_benchmark
    zmq_benchmark.cpp
)

target_include_directories(zmq_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(zmq_benchmark
    cppzmq
)

add_executable(zmq_large_message_benchmark
    zmq_large_message_benchmark.cpp
)

target_include_directories(zmq_large_message_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(zmq_large_message_benchmark
    cppzmq
)

add_executable(ipc_message_validation_test
    ipc_message_validation_test.cpp
)

target_include_directories(ipc_message_validation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ipc_message_validation_test
    ipc_core
)

add_executable(ipc_fragment_size_benchmark
    ipc_fragment_size_benchmark.cpp
)

target_include_directories(ipc_fragment_size_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ipc_fragment_size_benchmark
    ipc_core
)

add_executable(boost_message_size_limit_test
    boost_message_size_limit_test.cpp
)

target_include_directories(boost_message_size_limit_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    D:/local/vcpkg/installed/x64-windows/include
)

target_link_libraries(boost_message_size_limit_test
    Boost::boost
)

add_executable(zmq_ack_semantics_test
    zmq_ack_semantics_test.cpp
)

target_include_directories(zmq_ack_semantics_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(zmq_ack_semantics_test
    cppzmq
)

set_target_properties(
    ipc_multiclient_test
    ipc_large_message_benchmark
    ipc_small_message_benchmark
    ipc_message_validation_test
    ipc_fragment_size_benchmark
    boost_mq_benchmark
    boost_mq_large_message_benchmark
    zmq_benchmark
    zmq_large_message_benchmark
    boost_message_size_limit_test
    zmq_ack_semantics_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)