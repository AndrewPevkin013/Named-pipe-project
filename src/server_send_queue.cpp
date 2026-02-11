#include "server_send_queue.hpp"

ServerSendQueue::ServerSendQueue() {
    worker_ = std::thread(&ServerSendQueue::writer_loop, this);
}

ServerSendQueue::~ServerSendQueue() {
    stop();
}

void ServerSendQueue::stop() {
    running_ = false;
    cv_.notify_all();
    if (worker_.joinable())
        worker_.join();
}

void ServerSendQueue::push(HANDLE pipe, IPC::Fragment fragment) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push({ pipe, std::move(fragment) });
    }
    cv_.notify_one();
}

// void ServerSendQueue::push_immediate(HANDLE pipe, const IPC::Fragment& fragment) {
//     DWORD written = 0;
//     uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(fragment.data.size());

//     WriteFile(pipe, &packet_size, sizeof(packet_size), &written, nullptr);
//     WriteFile(pipe, &fragment.header, sizeof(fragment.header), &written, nullptr);

//     if (!fragment.data.empty()) {
//         WriteFile(pipe, fragment.data.data(), static_cast<DWORD>(fragment.data.size()), &written, nullptr);
//     }
// }

void ServerSendQueue::writer_loop() {
    while (running_ || !queue_.empty()) {
        ServerTask task;

        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return !queue_.empty() || !running_; });

            if (!running_ && queue_.empty())
                break;

            task = std::move(queue_.front());
            queue_.pop();
        }

        DWORD written = 0;
        uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(task.fragment.data.size());

        WriteFile(task.pipe, &packet_size, sizeof(packet_size), &written, nullptr);
        WriteFile(task.pipe, &task.fragment.header, sizeof(task.fragment.header), &written, nullptr);

        if (!task.fragment.data.empty()) {
            WriteFile(task.pipe, task.fragment.data.data(), static_cast<DWORD>(task.fragment.data.size()), &written, nullptr);
        }
    }
}