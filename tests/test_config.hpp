#pragma once
#include <cstddef>

namespace TestConfig {

    constexpr int CLIENT_COUNT = 1; 
    constexpr int MESSAGES_PER_CLIENT = 1;

    constexpr std::size_t MIN_MESSAGE_SIZE = 1024 * 1024 * 1024;
    constexpr std::size_t MAX_MESSAGE_SIZE = 1024 * 1024 * 1024;

    constexpr const char* LOG_DIR = "tests/logs/";
    constexpr const char* CLIENT_LOG_FILE = "tests/logs/client.log";
}
