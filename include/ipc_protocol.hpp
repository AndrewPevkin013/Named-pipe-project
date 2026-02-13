#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <map>
#include <atomic>

namespace IPC {
    #pragma pack(push, 1)

    struct FragmentHeader {
        uint32_t sender_id;
        uint64_t message_id;
        uint32_t total_size;
        uint32_t fragment_size;
        uint32_t fragment_index;
        uint32_t total_fragments;
        uint8_t flags;
        uint8_t reserved[15];

        bool is_last() const {
            return flags & 0x01;
        }
        void set_last(bool last) {
            if (last) flags |= 0x01;
            else flags &= ~0x01; 
        }
    };

    enum FragmentFlags : uint8_t {
        FLAG_DATA = 0x00,
        FLAG_LAST = 0x01,
        FLAG_ACK  = 0x02
    };
    
    constexpr size_t MIN_FRAGMENT_SIZE = 1;
    constexpr size_t MAX_FRAGMENT_SIZE = 64 * 1024;
    constexpr size_t HEADER_SIZE = sizeof(FragmentHeader);
    static_assert(sizeof(FragmentHeader) == HEADER_SIZE, "FragmentHeader size mismatch");

    struct Fragment {
        FragmentHeader header;
        std::vector<char> data;

        size_t total_packet_size() const {
            return HEADER_SIZE + data.size();
        }
    };

    #pragma pack(pop)

    struct AckPayload {
        uint64_t message_id;
    };

    class MessageFragmenter {
    public:
        MessageFragmenter() : next_message_id_(1) {}
        std::vector<Fragment> fragment_message(const std::vector<char>& message_data);
        std::vector<Fragment> fragment_message_with_size(const std::vector<char>& message_data, size_t fragment_size);
        Fragment create_control_message(const std::vector<char>& data, uint8_t flags = 0);
    
    private:
        size_t calculate_fragment_size(size_t total_message_size) const;
        std::atomic<uint64_t> next_message_id_;
    };

    class MessageAssembler {
    public:
        struct AssembledMessage {
            uint64_t message_id;
            std::vector<char> data;
            bool complete = false;
        };

        struct MessageKey {
            uint32_t sender_id;
            uint64_t message_id;

            bool operator<(const MessageKey& other) const {
                if (sender_id != other.sender_id)
                    return sender_id < other.sender_id;
                return message_id < other.message_id;
            }
        };
        
        bool add_fragment(const Fragment& fragment);
        AssembledMessage get_assembled_message(uint32_t sender_id, uint64_t message_id);
    
    private:
        struct AssemblyState {
            uint64_t message_id;
            uint32_t total_size;
            uint32_t total_fragments;
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