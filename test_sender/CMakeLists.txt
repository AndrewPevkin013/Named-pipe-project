add_executable(test_sender main.cpp)

target_link_libraries(test_sender PRIVATE ipc_protocol)

if(WIN32)
    target_link_libraries(test_sender PRIVATE kernel32)
endif()