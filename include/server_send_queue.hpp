#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <windows.h>

#include "ipc_protocol.hpp"

struct ServerTask {
    HANDLE pipe;
    IPC::Fragment fragment;
};

class ServerSendQueue {
public:
    ServerSendQueue();
    ~ServerSendQueue();

    void push(HANDLE pipe, IPC::Fragment fragment);
    // void push_immediate(HANDLE pipe, const IPC::Fragment& fragment);
    void stop();

private:
    void writer_loop();

    std::queue<ServerTask> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::atomic<bool> running_{true};
};