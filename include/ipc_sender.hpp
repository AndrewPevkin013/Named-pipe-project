#pragma once
#define _CRT_SECURE_NO_WARNINGS
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
    bool send_with_fragment_size(const std::string& message, size_t fragment_size); // для тестов

    // сделал методы публичными для тестов
    bool send_fragment(IPC::Fragment& fragment);
    bool send_fragment(const IPC::FragmentView& view);
    bool wait_for_ack(uint64_t expected_message_id);

    IPC::MessageFragmenter& get_fragmenter() { return fragmenter_; }
    
private:
    bool connect();
    // bool send_fragment(IPC::Fragment& fragment);
    // bool send_fragment(const IPC::FragmentView& view);
    // bool wait_for_ack(uint64_t expected_message_id);

    IPC::MessageFragmenter fragmenter_;
    uint32_t client_id_{0};

    #ifdef _WIN32
        HANDLE pipe_ = INVALID_HANDLE_VALUE;
    #else
        int pipe_ = -1;
    #endif
};