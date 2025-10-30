add_executable(test_sender main.cpp)

if(WIN32)
    target_link_libraries(test_sender PRIVATE kernel32 user32 advapi32)
endif()

set_target_properties(test_sender PROPERTIES
        OUTPUT_NAME "test_sender"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
