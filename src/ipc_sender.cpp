#include "../include/ipc_sender.hpp"
#include <vector>
#include <cstring>

#ifdef _WIN32
constexpr const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";
#else
constexpr const char* FIFO_PATH = "/tmp/ipc_test_fifo";
#endif

IPCSender::~IPCSender() {
    #ifdef _WIN32
        if (pipe_ != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe_);
        }
    #else
        if (pipe_ != -1) {
            close(pipe_);
        }
    #endif
}

IPCSender::IPCSender(uint32_t client_id) : client_id_(client_id), pipe_(INVALID_HANDLE_VALUE) {
    #ifdef _WIN32
        pipe_ = CreateFile(
            PIPE_NAME,
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
    #else
        pipe_ = open(FIFO_PATH, O_WRONLY);
    #endif
}


bool IPCSender::send_fragment(IPC::Fragment& fragment) {
    IPC::FragmentHeader header = fragment.header;
    fragment.header.sender_id = client_id_;

    uint32_t packet_size = sizeof(IPC::FragmentHeader) + static_cast<uint32_t>(fragment.data.size());

    DWORD written = 0;

    if (!WriteFile(pipe_, &packet_size, sizeof(packet_size), &written, nullptr) || written != sizeof(packet_size)) {
        return false;
    }

    if (!WriteFile(pipe_, &header, sizeof(header), &written, nullptr) || written != sizeof(header)) {
        return false;
    }

    if (!fragment.data.empty()) {
        if (!WriteFile(pipe_, fragment.data.data(), static_cast<DWORD>(fragment.data.size()), &written, nullptr) || written != fragment.data.size()) {
            return false;
        }
    }

    return true;
}



