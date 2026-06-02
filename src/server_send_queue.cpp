#include "server_send_queue.hpp"

namespace {

bool write_all(PipeHandle pipe, const void* data, size_t size) {
    size_t total = 0;
    const char* buf = static_cast<const char*>(data);

    while (total < size) {
#ifdef _WIN32
        DWORD written = 0;
        if (!WriteFile(pipe, buf + total, size - total, &written, nullptr))
            return false;
        total += written;
#else
        ssize_t written = write(pipe, buf + total, size - total);
        if (written <= 0)
            return false;
        total += written;
#endif
    }
    return true;
}

}

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

void ServerSendQueue::push(PipeHandle pipe, IPC::Fragment fragment) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push({ pipe, std::move(fragment) });
    }
    cv_.notify_one();
}

bool ServerSendQueue::write_fragment(PipeHandle pipe, const IPC::Fragment& fragment) {
#ifdef _WIN32
    if (pipe == INVALID_HANDLE_VALUE) return false;
#else
    if (pipe == -1) return false;
#endif

    uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(fragment.data.size());

    if (!write_all(pipe, &packet_size, sizeof(packet_size)))
        return false;

    if (!write_all(pipe, &fragment.header, sizeof(fragment.header)))
        return false;

    if (!fragment.data.empty()) {
        if (!write_all(pipe, fragment.data.data(), fragment.data.size()))
            return false;
    }

    return true;
}

void ServerSendQueue::push_immediate(PipeHandle pipe, const IPC::Fragment& fragment) {
    if (!write_fragment(pipe, fragment)) {
#ifdef _WIN32
        DWORD err = GetLastError();
        std::cout << "[ERROR] ACK write failed. err=" << err << "\n";
#else
        perror("[ERROR] ACK write failed");
#endif
        return;
    }

    // std::cout << "[ACK SENT] msg_id=" << fragment.header.message_id << "\n";
}


void ServerSendQueue::writer_loop() {
    while (running_ || !queue_.empty()) {
        ServerTask task;

        {
            std::unique_lock lock(mutex_);

            cv_.wait(lock, [&] {
                return !queue_.empty() || !running_;
            });

            if (!running_ && queue_.empty())
                break;

            task = std::move(queue_.front());
            queue_.pop();
        }

        if (!write_fragment(task.pipe, task.fragment)) {
#ifdef _WIN32
            DWORD err = GetLastError();
            std::cout << "[ERROR] Write failed. err=" << err << "\n";
#else
            perror("[ERROR] Write failed");
#endif
        }
    }
}