cmake_minimum_required(VERSION 3.10)
project(CrossPlatformIPC VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(ipc_protocol STATIC
    src/fragment_manager.cpp
    src/message_assembler.cpp
)

target_include_directories(ipc_protocol PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(functional_tests tests/functional_tests.cpp)
target_link_libraries(functional_tests ipc_protocol)
set_target_properties(functional_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests)

add_subdirectory(test_sender)
add_subdirectory(test_receiver)