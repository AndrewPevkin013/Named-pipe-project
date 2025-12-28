cmake_minimum_required(VERSION 3.10)
project(CrossPlatformIPC VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(ipc_protocol STATIC
    src/fragment_manager.cpp
    src/message_assembler.cpp
)

add_subdirectory(test_sender)
add_subdirectory(test_receiver)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)