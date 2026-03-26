#pragma once

#include "server_send_queue.hpp"
#include "ipc_protocol.hpp"

#include <atomic>
#include <thread>
#include <fstream>
#include <filesystem>
#include <functional>
#include <mutex>
#include <iostream>
#include <string>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <cstring>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

class IPCReceiver {
public:
    IPCReceiver();
    ~IPCReceiver();

    static std::atomic<uint32_t> next_client_id;
    const char* SERVER_LOG_DIR  = "logs";
    const char* SERVER_LOG_FILE = "logs/server.log";
    std::mutex log_mutex;
    std::ofstream server_log;
    void run();

    using Message = IPC::MessageAssembler::AssembledMessage;
    using MessageHandler = std::function<void(const Message&)>;
    void set_message_handler(MessageHandler handler);

private:
    MessageHandler message_handler_;
    void log_line(const std::string& line);
    ServerSendQueue send_queue_;
    std::atomic<bool> running_;
    void client_loop(PipeHandle pipe);
    bool receive(PipeHandle pipe, IPC::Fragment& fragment);
    void send_ack(PipeHandle pipe, uint64_t msg_id, uint32_t /*client_id*/);

#ifdef _WIN32
    using PipeHandle = HANDLE;
    const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";
    PipeHandle pipe = INVALID_HANDLE_VALUE;
#else
    using PipeHandle = int;
    const char* CONNECT_PIPE = "/tmp/ipc_connect";
#endif
};