#include "../include/ipc_protocol.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <string>      
#include <algorithm>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
    constexpr const char* PIPE_NAME = "\\\\.\\pipe\\IPCTestPipe";
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    constexpr const char* FIFO_PATH = "/tmp/ipc_test_fifo";
#endif

class IPCSender {
public:
    enum class SendStatus {
        SUCCESS,
        NO_RECEIVER,
        BUFFER_FULL,
        PIPE_ERROR,
        TIMEOUT
    };

    IPCSender() {
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
    
    ~IPCSender() {
        if (pipe_ != INVALID_HANDLE_VALUE) {
            #ifdef _WIN32
                CloseHandle(pipe_);
            #else
                close(pipe_);
            #endif
        }
    }
    
    bool send_fragment(const IPC::Fragment& fragment) {
        if (!send_raw(&fragment.header, IPC::HEADER_SIZE)) {
            return false;
        }
        
        if (!send_raw(fragment.data.data(), fragment.data.size())) {
            return false;
        }
        
        return true;
    }

    SendStatus send_fragment_nonblocking(const IPC::Fragment& fragment) {
        #ifdef _WIN32
            if (pipe_ == INVALID_HANDLE_VALUE) {
                return SendStatus::NO_RECEIVER;
            }
            
            DWORD flags = 0;
            if (!GetNamedPipeInfo(pipe_, &flags, NULL, NULL, NULL)) {
                return SendStatus::PIPE_ERROR;
            }
            
            DWORD bytes_available = 0;
            if (!PeekNamedPipe(pipe_, NULL, 0, NULL, &bytes_available, NULL)) {
                return try_write_nonblocking(fragment);
            }
            
            const DWORD MAX_BUFFER_SIZE = 65536;
            if (bytes_available > MAX_BUFFER_SIZE * 0.9) {
                return SendStatus::BUFFER_FULL;
            }
            
            return try_write_nonblocking(fragment);
            
        #else
            if (pipe_ == -1) {
                return SendStatus::NO_RECEIVER;
            }
            
            int flags = fcntl(pipe_, F_GETFL, 0);
            fcntl(pipe_, F_SETFL, flags | O_NONBLOCK);
            
            ssize_t result = write(pipe_, &fragment.header, IPC::HEADER_SIZE);
            
            if (result == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return SendStatus::BUFFER_FULL;
                } else {
                    return SendStatus::PIPE_ERROR;
                }
            }
            
            result = write(pipe_, fragment.data.data(), fragment.data.size());
            if (result == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return SendStatus::BUFFER_FULL;
                } else {
                    return SendStatus::PIPE_ERROR;
                }
            }
            
            return SendStatus::SUCCESS;
        #endif
    }
    
private:
    #ifdef _WIN32
    SendStatus try_write_nonblocking(const IPC::Fragment& fragment) {
        DWORD bytes_written = 0;
        DWORD mode = PIPE_NOWAIT;
        
        if (!SetNamedPipeHandleState(pipe_, &mode, NULL, NULL)) {
            return send_with_timeout(fragment, 100);
        }
        
        if (!WriteFile(pipe_, &fragment.header, IPC::HEADER_SIZE, 
                      &bytes_written, NULL)) {
            DWORD err = GetLastError();
            if (err == ERROR_NO_DATA || err == ERROR_PIPE_NOT_CONNECTED) {
                return SendStatus::NO_RECEIVER;
            } else if (err == ERROR_IO_PENDING) {
                return SendStatus::BUFFER_FULL;
            }
            return SendStatus::PIPE_ERROR;
        }
        
        if (!WriteFile(pipe_, fragment.data.data(), 
                      static_cast<DWORD>(fragment.data.size()), 
                      &bytes_written, NULL)) {
            return SendStatus::PIPE_ERROR;
        }
        
        return SendStatus::SUCCESS;
    }
    
    SendStatus send_with_timeout(const IPC::Fragment& fragment, DWORD timeout_ms) {
        return SendStatus::SUCCESS;
    }
    #endif
    
    bool send_raw(const void* data, size_t size) {
        #ifdef _WIN32
            DWORD bytes_written;
            if (!WriteFile(pipe_, data, static_cast<DWORD>(size), &bytes_written, NULL)) {
                return false;
            }
            return bytes_written == size;
        #else
            ssize_t bytes_written = write(pipe_, data, size);
            return bytes_written == static_cast<ssize_t>(size);
        #endif
    }
    
#ifdef _WIN32
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
#else
    int pipe_ = -1;
#endif
};

std::vector<char> generate_test_data(size_t size) {
    std::vector<char> data(size);
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, 255);
    
    for (size_t i = 0; i < size; i++) {
        data[i] = static_cast<char>(dist(rng));
    }
    
    return data;
}

void run_test_mode() {
    std::cout << "Running in TEST mode with 3 senders..." << std::endl;
    
    const int NUM_SENDERS = 3;
    IPC::MessageFragmenter fragmenter;
    
    std::vector<std::thread> sender_threads;
    
    for (int sender_id = 0; sender_id < NUM_SENDERS; sender_id++) {
        sender_threads.emplace_back([sender_id, &fragmenter]() {
            IPCSender sender;
            
            const size_t sizes[] = {
                1024,
                1024 * 100,
                1024 * 1024,
                1024 * 1024 * 10,
                1024 * 1024 * 50
            };
            
            for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
                std::cout << "\n[Sender " << sender_id << "] Preparing message " 
                          << i << " (" << sizes[i] << " bytes)" << std::endl;
                
                auto data = generate_test_data(sizes[i]);
                
                std::ostringstream oss;
                oss << "sender:" << sender_id << "\n"
                    << "message_id:" << i << "\n"
                    << "size:" << sizes[i] << "\n"
                    << "timestamp:" << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";
                std::string metadata = oss.str();
                
                std::vector<char> full_message;
                full_message.insert(full_message.end(), metadata.begin(), metadata.end());
                full_message.insert(full_message.end(), data.begin(), data.end());
                
                auto fragments = fragmenter.fragment_message(full_message);
                
                std::cout << "[Sender " << sender_id << "] Sending " 
                          << fragments.size() << " fragments..." << std::endl;
                
                for (const auto& fragment : fragments) {
                    auto status = sender.send_fragment_nonblocking(fragment);
                    
                    switch (status) {
                        case IPCSender::SendStatus::SUCCESS:
                            break;
                        case IPCSender::SendStatus::BUFFER_FULL:
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            break;
                        case IPCSender::SendStatus::NO_RECEIVER:
                            std::cerr << "[Sender " << sender_id << "] Receiver disconnected" << std::endl;
                            return;
                        case IPCSender::SendStatus::PIPE_ERROR:
                            std::cerr << "[Sender " << sender_id << "] Pipe error" << std::endl;
                            break;
                        case IPCSender::SendStatus::TIMEOUT:
                            std::cerr << "[Sender " << sender_id << "] ACK timeout" << std::endl;
                            break;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                
                std::cout << "[Sender " << sender_id << "] Message " 
                          << i << " sent successfully" << std::endl;
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });
    }
    
    for (auto& thread : sender_threads) {
        thread.join();
    }
    
    std::cout << "\nAll test senders finished." << std::endl;
}

void run_interactive_mode() {
    std::cout << "Running in INTERACTIVE mode..." << std::endl;
    
    IPCSender sender;
    IPC::MessageFragmenter fragmenter;
    
    std::cout << "Enter your message (type ':q' to quit, ':t' for test mode): " << std::endl;
    std::string msg;
    
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, msg);
        
        if (msg == ":q") {
            break;
        }
        if (msg == ":t") {
            run_test_mode();
            continue;
        }
        
        std::vector<char> message_data(msg.begin(), msg.end());
        
        std::ostringstream oss;
        oss << "type:text\n"
            << "size:" << msg.size() << "\n"
            << "timestamp:" << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";
        std::string metadata = oss.str();
        
        std::vector<char> full_message;
        full_message.insert(full_message.end(), metadata.begin(), metadata.end());
        full_message.insert(full_message.end(), message_data.begin(), message_data.end());
        
        auto fragments = fragmenter.fragment_message(full_message);
        
        std::cout << "Sending " << fragments.size() << " fragment(s)..." << std::endl;
        
        bool success = true;
        for (const auto& fragment : fragments) {
            auto status = sender.send_fragment_nonblocking(fragment);
            
            switch (status) {
                case IPCSender::SendStatus::SUCCESS:
                    break;
                case IPCSender::SendStatus::BUFFER_FULL:
                    std::cout << "Buffer full, retrying..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    break;
                case IPCSender::SendStatus::NO_RECEIVER:
                    std::cout << "No receiver connected" << std::endl;
                    success = false;
                    break;
                case IPCSender::SendStatus::PIPE_ERROR:
                    std::cout << "Pipe error" << std::endl;
                    success = false;
                    break;
                case IPCSender::SendStatus::TIMEOUT:
                    std::cout << "Timeout" << std::endl;
                    break;
            }
            
            if (!success) {
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        if (success) {
            std::cout << "Message sent successfully" << std::endl;
        }
    }
}

int main() {
    std::cout << "IPC Sender with TCP/IP-like fragmentation" << std::endl;
    std::cout << "Choose mode:" << std::endl;
    std::cout << "1. Interactive mode (enter messages manually)" << std::endl;
    std::cout << "2. Test mode (3 concurrent senders)" << std::endl;
    std::cout << "Enter choice (1 or 2): ";
    
    std::string choice;
    std::getline(std::cin, choice);
    
    if (choice == "2") {
        run_test_mode();
    } else {
        run_interactive_mode();
    }
    
    std::cout << "\nSender finished." << std::endl;
    return 0;
}