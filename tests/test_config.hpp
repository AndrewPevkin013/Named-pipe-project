#pragma once
#include <cstddef>

namespace TestConfig {

    // ===== НАГРУЗКА =====
    constexpr int CLIENT_COUNT = 10;              // увеличивай до 50–100
    constexpr int MESSAGES_PER_CLIENT = 50;

    constexpr std::size_t MIN_MESSAGE_SIZE = 1 * 1024;        // 1 KB
    constexpr std::size_t MAX_MESSAGE_SIZE = 64 * 1024 * 1024; // 64 MB (начни с этого!)

    // ===== ACK =====
    constexpr int ACK_TIMEOUT_MS = 5000;

    // ===== ЛОГИ =====
    constexpr const char* LOG_DIR = "tests/logs/";
    constexpr const char* CLIENT_LOG_FILE = "tests/logs/client.log";
    constexpr const char* SERVER_LOG_FILE = "tests/logs/server.log";
}
