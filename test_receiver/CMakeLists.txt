add_executable(test_receiver main.cpp)

target_link_libraries(test_receiver PRIVATE ipc_protocol)

if(WIN32)
    target_link_libraries(test_receiver PRIVATE kernel32)
endif()