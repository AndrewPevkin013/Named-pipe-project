#pragma once

#include "ipc_protocol.hpp"

#include <iostream>
#include <string>
#include <vector>


#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

class IPCSender {
public:
    explicit IPCSender();
    ~IPCSender();

    bool send(const std::string& message);
    
private:
    bool connect();
    bool send_fragment(IPC::Fragment& fragment);
    bool wait_for_ack(uint64_t expected_message_id);

    IPC::MessageFragmenter fragmenter_;
    uint32_t client_id_{0};

    #ifdef _WIN32
        HANDLE pipe_ = INVALID_HANDLE_VALUE;
    #else
        int pipe_ = -1;
    #endif
};