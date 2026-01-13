#include "../include/ipc_send_queue.hpp"
#include "../include/ipc_sender.hpp"

#include <iostream>


IPCSendQueue::IPCSendQueue(uint32_t client_id) : client_id_(client_id) {
    writer_thread_ = std::thread(&IPCSendQueue::writer_loop, this);
}


IPCSendQueue::~IPCSendQueue() {
    stop();
}

void IPCSendQueue::stop() {
    running_ = false;
    cv_.notify_all();

    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
}

void IPCSendQueue::push(IPC::Fragment fragment) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        fragment.header.sender_id = client_id_;
        queue_.push(std::move(fragment));
    }
    cv_.notify_one();
}

void IPCSendQueue::writer_loop() {
    IPCSender sender(client_id_);
    while (true) {
        IPC::Fragment fragment;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] {
                return !queue_.empty() || !running_;
            });

            if (!running_ && queue_.empty()) {
                break;
            }

            fragment = std::move(queue_.front());
            queue_.pop();
        }

        if (!sender.send_fragment(fragment)) {
            std::cerr << "[SendQueue] Failed to send fragment " << "(msg=" << fragment.header.message_id << ", idx=" << fragment.header.fragment_index << ")\n";
        }
    }
}
