#include "../include/ipc_receiver.hpp"

std::atomic<uint32_t> IPCReceiver::next_client_id{1};

void IPCReceiver::run() {
    log_line("[Receiver] Waiting for connections....");

#ifdef _WIN32
    while (running_) {
        pipe = CreateNamedPipeA(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            64 * 1024,
            64 * 1024,
            0,
            nullptr
        );

        if (pipe == INVALID_HANDLE_VALUE)
            continue;

        if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(pipe);
            continue;
        }

        std::thread(&IPCReceiver::client_loop, this, pipe).detach();
    }

#else
    if (mkfifo(PIPE_NAME, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo");
        return;
    }

    while (running_) {
        int fd = open(PIPE_NAME, O_RDWR);
        if (fd < 0)
            continue;

        std::thread(&IPCReceiver::client_loop, this, fd).detach();
    }
#endif
}   

void IPCReceiver::set_message_handler(MessageHandler handler) {
    message_handler_ = std::move(handler);
}

IPCReceiver::IPCReceiver() : running_(true) {
    std::filesystem::create_directories(SERVER_LOG_DIR);
    server_log.open(SERVER_LOG_FILE, std::ios::out | std::ios::trunc);

    if (!server_log) {
        std::cerr << "[Server] Failed to open log file\n";
    }
}

IPCReceiver::~IPCReceiver() {
#ifdef _WIN32
    if (pipe != INVALID_HANDLE_VALUE)
        CloseHandle(pipe);
#endif
}

void IPCReceiver::log_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << line << std::endl;
    if (server_log.is_open()) {
        server_log << line << std::endl;
        server_log.flush();
    }
}

void IPCReceiver::client_loop(HANDLE pipe) {
    uint32_t client_id = next_client_id++;
    IPC::MessageAssembler assembler;
    log_line("\n[CONNECT] client_id=" + std::to_string(client_id));

    while (running_) {
        IPC::Fragment fragment;
        if (!receive(pipe, fragment))
            break;

        if (assembler.add_fragment(fragment)) {
            auto msg = assembler.get_assembled_message(
                fragment.header.sender_id,
                fragment.header.message_id
            );

            if (msg.complete) {
                log_line(
                    "[RECEIVED] client=" + std::to_string(client_id) +
                    " msg_id=" + std::to_string(fragment.header.message_id) +
                    " size=" + std::to_string(msg.data.size())
                );

                if (message_handler_) {
                    message_handler_(msg);
                }

                send_ack(pipe, fragment.header.message_id, client_id);
            }
        }
    }

    log_line("\n[DISCONNECT] client_id=" + std::to_string(client_id));
#ifdef _WIN32
    CloseHandle(pipe);
#else
    close(pipe);
#endif
}

bool IPCReceiver::receive(PipeHandle pipe, IPC::Fragment& fragment) {
#ifdef _WIN32
    DWORD read = 0;
    uint32_t packet_size = 0;

    if (!ReadFile(pipe, &packet_size, sizeof(packet_size), &read, nullptr))
        return false;

    if (!ReadFile(pipe, &fragment.header, sizeof(fragment.header), &read, nullptr))
        return false;

    fragment.data.resize(fragment.header.fragment_size);
    if (!fragment.data.empty()) {
        if (!ReadFile(
                pipe,
                fragment.data.data(),
                fragment.header.fragment_size,
                &read,
                nullptr))
            return false;
    }

    return true;

#else
    ssize_t read_bytes;
    uint32_t packet_size = 0;

    read_bytes = read(pipe, &packet_size, sizeof(packet_size));
    if (read_bytes <= 0)
        return false;

    read_bytes = read(pipe, &fragment.header, sizeof(fragment.header));
    if (read_bytes <= 0)
        return false;

    fragment.data.resize(fragment.header.fragment_size);

    if (!fragment.data.empty()) {
        read_bytes = read(pipe, fragment.data.data(), fragment.header.fragment_size);
        if (read_bytes <= 0)
            return false;
    }

    return true;
#endif
}

void IPCReceiver::send_ack(HANDLE pipe, uint64_t msg_id, uint32_t /*client_id*/) {
    IPC::Fragment ack{};
    ack.header.sender_id = 0;
    ack.header.message_id = msg_id;
    ack.header.total_size = sizeof(IPC::AckPayload);
    ack.header.total_fragments = 1;
    ack.header.fragment_index = 0;
    ack.header.fragment_size = sizeof(IPC::AckPayload);
    ack.header.flags = IPC::FLAG_ACK | IPC::FLAG_LAST;

    IPC::AckPayload payload{ msg_id };
    ack.data.resize(sizeof(payload));
    memcpy(ack.data.data(), &payload, sizeof(payload));

    send_queue_.push_immediate(pipe, ack);
}
