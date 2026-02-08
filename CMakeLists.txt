cmake_minimum_required(VERSION 3.10)
project(CrossPlatformIPC VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(ipc_core STATIC
    src/fragment_manager.cpp
    src/message_assembler.cpp
    src/server_send_queue.cpp
    src/ipc_sender.cpp
)

target_include_directories(ipc_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

if (WIN32)
    target_link_libraries(ipc_core PUBLIC kernel32)
endif()

add_subdirectory(test_sender)
add_subdirectory(test_receiver)
add_subdirectory(tests)