#pragma once

#include "ipc_protocol.hpp"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

class IPCSender {
public:
    explicit IPCSender(uint32_t client_id);
    ~IPCSender();

    bool send_fragment(IPC::Fragment& fragment);
    bool wait_for_ack(uint64_t expected_message_id);
    
private:
    uint32_t client_id_;
    #ifdef _WIN32
        HANDLE pipe_ = INVALID_HANDLE_VALUE;
    #else
        int pipe_ = -1;
    #endif
};