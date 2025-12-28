#include "../include/ipc_protocol.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <sstream>
#include <chrono>

#ifdef _WIN32
    #include <windows.h>
    constexpr const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    constexpr const char* FIFO_PATH = "/tmp/ipc_test_fifo";
#endif

class IPCReceiver {
public:
    IPCReceiver() : running_(true) {
        #ifdef _WIN32
            pipe_ = CreateNamedPipe(
                PIPE_NAME,
                PIPE_ACCESS_INBOUND,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                4096,
                4096,
                0,
                NULL
            );
        #else
            mkfifo(FIFO_PATH, 0666);
            pipe_ = open(FIFO_PATH, O_RDONLY);
        #endif
    }
    
    ~IPCReceiver() {
        running_ = false;
        if (pipe_ != INVALID_HANDLE_VALUE) {
            #ifdef _WIN32
                CloseHandle(pipe_);
            #else
                close(pipe_);
                unlink(FIFO_PATH);
            #endif
        }
    }
    
    bool receive_fragment(IPC::Fragment& fragment) {
        if (!receive_raw(&fragment.header, IPC::HEADER_SIZE)) {
            return false;
        }
        
        fragment.data.resize(fragment.header.fragment_size);
        if (!receive_raw(fragment.data.data(), fragment.data.size())) {
            return false;
        }
        
        return true;
    }
    
    bool is_valid() const {
        return pipe_ != INVALID_HANDLE_VALUE;
    }
    
private:
    bool receive_raw(void* buffer, size_t size) {
        #ifdef _WIN32
            DWORD bytes_read;
            if (!ReadFile(pipe_, buffer, static_cast<DWORD>(size), &bytes_read, NULL)) {
                return false;
            }
            return bytes_read == size;
        #else
            ssize_t bytes_read = read(pipe_, buffer, size);
            return bytes_read == static_cast<ssize_t>(size);
        #endif
    }
    
#ifdef _WIN32
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
#else
    int pipe_ = -1;
#endif
    std::atomic<bool> running_;
};

void message_callback(const IPC::MessageAssembler::AssembledMessage& message) {
    if (!message.complete || message.data.empty()) {
        return;
    }
    
    std::string data_str(message.data.begin(), message.data.end());
    
    size_t metadata_end = data_str.find("\n\n");
    if (metadata_end != std::string::npos) {
        std::string metadata = data_str.substr(0, metadata_end);
        std::cout << "\n[Callback] Received message ID: " << message.message_id << std::endl;
        std::cout << "Metadata:\n" << metadata << std::endl;
        
        std::istringstream stream(metadata);
        std::string line;
        while (std::getline(stream, line)) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                std::cout << "  " << key << " = " << value << std::endl;
            }
        }
        
        size_t body_size = message.data.size() - metadata_end - 2;
        std::cout << "Body size: " << body_size << " bytes" << std::endl;
        
    }
}

int main() {
    std::cout << "IPC Receiver with TCP/IP-like reassembly" << std::endl;
    
    IPCReceiver receiver;
    if (!receiver.is_valid()) {
        std::cerr << "Failed to initialize receiver!" << std::endl;
        return 1;
    }
    
    IPC::MessageAssembler assembler;
    std::atomic<bool> running = true;
    
    std::thread cleaner([&assembler, &running]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            assembler.cleanup_old_messages();
        }
    });
    
    std::cout << "Waiting for messages..." << std::endl;
    
    size_t total_fragments_received = 0;
    size_t total_messages_received = 0;
    
    try {
        while (running) {
            IPC::Fragment fragment;
            
            if (receiver.receive_fragment(fragment)) {
                total_fragments_received++;
                
                bool complete = assembler.add_fragment(fragment);
                
                if (complete) {
                    auto message = assembler.get_assembled_message(
                        fragment.header.message_id);
                    
                    if (message.complete) {
                        total_messages_received++;
                        
                        message_callback(message);
                        
                        std::cout << "\n[Receiver] Total: " 
                                  << total_messages_received << " messages, "
                                  << total_fragments_received << " fragments received" << std::endl;
                    }
                }
                
                if (total_fragments_received % 100 == 0) {
                    std::cout << "[Receiver] Progress: " 
                              << total_fragments_received << " fragments received" << std::endl;
                }
                
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    running = false;
    cleaner.join();
    
    std::cout << "\nReceiver shutdown. Final stats:" << std::endl;
    std::cout << "Total fragments received: " << total_fragments_received << std::endl;
    std::cout << "Total messages received: " << total_messages_received << std::endl;
    
    return 0;
}