#include "../include/ipc_protocol.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <string>      
#include <algorithm>

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
    
private:
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

int main() {
    std::cout << "IPC Sender with TCP/IP-like fragmentation" << std::endl;
    
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
                
                std::string metadata = 
                    "sender:" + std::to_string(sender_id) + "\n" +
                    "message_id:" + std::to_string(i) + "\n" +
                    "size:" + std::to_string(sizes[i]) + "\n" +
                    "timestamp:" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "\n\n";
                
                std::vector<char> full_message;
                full_message.insert(full_message.end(), metadata.begin(), metadata.end());
                full_message.insert(full_message.end(), data.begin(), data.end());
                
                auto fragments = fragmenter.fragment_message(full_message);
                
                std::cout << "[Sender " << sender_id << "] Sending " 
                          << fragments.size() << " fragments..." << std::endl;
                
                for (const auto& fragment : fragments) {
                    if (!sender.send_fragment(fragment)) {
                        std::cerr << "[Sender " << sender_id 
                                  << "] Failed to send fragment!" << std::endl;
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
    
    std::cout << "\nAll senders finished." << std::endl;
    return 0;
}