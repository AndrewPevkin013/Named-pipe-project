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
    #include <sys/stat.h>
#endif

class IPCSender {
public:
    explicit IPCSender(uint32_t client_id = 0);
    ~IPCSender();

    bool send(const std::string& message);
    bool send_with_fragment_size(const std::string& message, size_t fragment_size); // для тестов

    // сделал методы публичными для тестов
    bool send_fragment(IPC::Fragment& fragment);
    bool send_fragment(const IPC::FragmentView& view);
    bool wait_for_ack(uint32_t expected_message_id);
    void disconnect();

    IPC::MessageFragmenter& get_fragmenter() { return fragmenter_; }
    
private:
    bool connect();
    static std::atomic<uint32_t> next_sender_id_;
    std::string in_fifo;
    std::string out_fifo;
    int write_fd_ = -1;
    int read_fd_ = -1;
    // bool send_fragment(IPC::Fragment& fragment);
    // bool send_fragment(const IPC::FragmentView& view);
    // bool wait_for_ack(uint64_t expected_message_id);

    IPC::MessageFragmenter fragmenter_;
    uint16_t client_id_{0};
    std::mutex send_mutex_;

    #ifdef _WIN32
        HANDLE pipe_ = INVALID_HANDLE_VALUE;
    #else
        int pipe_ = -1;
    #endif
};