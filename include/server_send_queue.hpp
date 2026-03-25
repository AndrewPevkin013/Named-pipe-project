#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <iostream>

#include "ipc_protocol.hpp"

#ifdef _WIN32
    #include <windows.h>
    using PipeHandle = HANDLE;
#else
    #include <unistd.h>
    using PipeHandle = int;
#endif

struct ServerTask {
    PipeHandle pipe;
    IPC::Fragment fragment;
};

class ServerSendQueue {
public:
    ServerSendQueue();
    ~ServerSendQueue();
    void push(PipeHandle pipe, IPC::Fragment fragment);
    void push_immediate(PipeHandle pipe, const IPC::Fragment& fragment);
    void stop();

private:
    void writer_loop();
    bool write_fragment(PipeHandle pipe, const IPC::Fragment& fragment);
    std::queue<ServerTask> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::atomic<bool> running_{true};
};