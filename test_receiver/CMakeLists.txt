add_executable(test_receiver main.cpp)

if(WIN32)
    target_link_libraries(test_receiver PRIVATE kernel32 user32 advapi32)
endif()

set_target_properties(test_receiver PROPERTIES
        OUTPUT_NAME "test_receiver"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)