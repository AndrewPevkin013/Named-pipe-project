cmake_minimum_required(VERSION 3.10)

project(NamedPipeDemo VERSION 1.0.0)
project(NamedPipeDemo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(test_sender)
add_subdirectory(test_receiver)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)