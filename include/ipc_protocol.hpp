#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <map>
#include <atomic>
#include <functional>
#include <optional>

namespace IPC {
    #pragma pack(push, 1)

    struct FragmentHeader {
        uint16_t sender_id;
        uint32_t message_id;
        uint32_t total_size;
        uint32_t fragment_size;
        uint32_t fragment_index;
        uint32_t total_fragments;
        uint32_t message_checksum;
        uint8_t flags;
        uint8_t reserved[7];

        void set_last(bool last) {
            if (last) flags |= 0x01;
            else flags &= ~0x01; 
        }
    };

    enum FragmentFlags : uint8_t {
        FLAG_DATA = 0x00,
        FLAG_LAST = 0x01,
        FLAG_ACK  = 0x02,
        FLAG_NACK = 0x04
    };
    
    constexpr size_t MIN_FRAGMENT_SIZE = 1;
    // constexpr size_t MAX_FRAGMENT_SIZE = 64 * 1024;
    constexpr size_t MAX_FRAGMENT_SIZE = 100 * 1024 * 1024; // изменил порог для проверки передачи фрагментов по 50 МБ (для тестов)
    constexpr size_t HEADER_SIZE = sizeof(FragmentHeader);
    static_assert(sizeof(FragmentHeader) == HEADER_SIZE, "FragmentHeader size mismatch");

    struct Fragment {
        FragmentHeader header;
        std::vector<char> data;
    };

    #pragma pack(pop)

    struct AckPayload {
        uint32_t message_id;
    };

    class FragmentView {
    public:
        FragmentView(const FragmentHeader& h, const char* d, size_t s) 
            : header_(h), data_(d), size_(s) {}
        
        const FragmentHeader& header() const { return header_; }
        const char* data() const { return data_; }
        size_t size() const { return size_; }
        
        Fragment to_fragment() const {
            Fragment f;
            f.header = header_;
            f.data.assign(data_, data_ + size_);
            return f;
        }
        
    private:
        FragmentHeader header_;
        const char* data_;
        size_t size_;
    };

    using FragmentCallback = std::function<bool(const FragmentView&)>;


    class MessageFragmenter {
    public:
        MessageFragmenter() : next_message_id_(1) {}
        std::vector<Fragment> fragment_message(const std::vector<char>& message_data);
        std::vector<Fragment> fragment_message_with_size(const std::vector<char>& message_data, size_t fragment_size);
        Fragment create_control_message(const std::vector<char>& data, uint8_t flags = 0);
        bool fragment_message_stream(const std::vector<char>& message_data, FragmentCallback callback);
        bool fragment_message_stream_with_size(const std::vector<char>& message_data, size_t fragment_size, FragmentCallback callback);
    
    private:
        size_t calculate_fragment_size(size_t total_message_size) const;
        void fill_fragment(Fragment& f, uint32_t message_id, size_t total_size, size_t fragment_size, size_t fragment_index, size_t total_fragments, const std::vector<char>& message_data);
        std::atomic<uint32_t> next_message_id_;
    };

    class MessageAssembler {
    public:
        struct AssembledMessage {
            uint32_t message_id;
            std::vector<char> data;
            bool complete = false;
        };

        struct MessageKey {
            uint16_t sender_id;
            uint32_t message_id;

            bool operator<(const MessageKey& other) const {
                if (sender_id != other.sender_id)
                    return sender_id < other.sender_id;
                return message_id < other.message_id;
            }
        };
        
        bool add_fragment(const Fragment& fragment);
        AssembledMessage get_assembled_message(uint16_t sender_id, uint32_t message_id);
    
    private:
        struct AssemblyState {
            uint64_t message_id;
            uint32_t total_size;
            uint32_t total_fragments;
            uint32_t message_checksum = 0;
            std::vector<std::vector<char>> fragments;
            std::vector<bool> received;
            uint32_t received_count = 0;
            std::chrono::steady_clock::time_point created_at;
        
            bool is_complete() const {
                return received_count == total_fragments;
            }
        };
        std::map<MessageKey, AssemblyState> assembly_map_;
        mutable std::mutex mutex_;
    };
}