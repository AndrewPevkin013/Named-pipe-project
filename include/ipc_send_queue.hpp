#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "ipc_protocol.hpp"

class IPCSendQueue {
public:
    ~IPCSendQueue();

    explicit IPCSendQueue(uint32_t client_id);
    IPCSendQueue(const IPCSendQueue&) = delete;
    IPCSendQueue& operator=(const IPCSendQueue&) = delete;

    void push(IPC::Fragment fragment);
    void stop();

private:
    void writer_loop();
    uint32_t client_id_;
    std::queue<IPC::Fragment> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread writer_thread_;
    std::atomic<bool> running_{true};
};
