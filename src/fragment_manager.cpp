#include "../include/ipc_protocol.hpp"
#include <algorithm>
#include <iostream>
#include <atomic>

namespace IPC {

size_t MessageFragmenter::calculate_fragment_size(size_t total_message_size) const {
    
    if (total_message_size <= 1024) {
        return total_message_size;
    }
    else if (total_message_size <= 1024 * 1024) {
        return 16 * 1024;
    }
    else if (total_message_size <= 10 * 1024 * 1024) {
        return 32 * 1024;
    }
    else {                                          
        return 64 * 1024;
    }
}

std::vector<Fragment> MessageFragmenter::fragment_message(const std::vector<char>& message_data) {
    size_t fragment_size = calculate_fragment_size(message_data.size());
    
    return fragment_message_with_size(message_data, fragment_size);
}

std::vector<Fragment> MessageFragmenter::fragment_message_with_size(const std::vector<char>& message_data, size_t fragment_size) {
    fragment_size = std::clamp(fragment_size, MIN_FRAGMENT_SIZE, MAX_FRAGMENT_SIZE);

    uint64_t message_id = next_message_id_++;
    size_t total_size = message_data.size();
    size_t total_fragments = (total_size + fragment_size - 1) / fragment_size;

    std::vector<Fragment> fragments;
    fragments.reserve(total_fragments);

    for (size_t i = 0; i < total_fragments; ++i) {
        size_t offset = i * fragment_size;
        size_t size = std::min(fragment_size, total_size - offset);

        Fragment f;
        f.header.message_id = message_id;
        f.header.total_size = static_cast<uint32_t>(total_size);
        f.header.fragment_size = static_cast<uint32_t>(size);
        f.header.fragment_index = static_cast<uint32_t>(i);
        f.header.total_fragments = static_cast<uint32_t>(total_fragments);
        f.header.flags = (i + 1 == total_fragments) ? FLAG_LAST : FLAG_DATA;

        f.data.insert(f.data.end(), message_data.begin() + offset, message_data.begin() + offset + size);

        fragments.push_back(std::move(f));
    }

    return fragments;
}


Fragment MessageFragmenter::create_control_message(const std::vector<char>& data, uint8_t flags) {
    
    Fragment fragment;
    fragment.header.message_id = next_message_id_++;
    fragment.header.total_size = static_cast<uint32_t>(data.size());
    fragment.header.fragment_size = static_cast<uint32_t>(data.size());
    fragment.header.fragment_index = 0;
    fragment.header.total_fragments = 1;
    fragment.header.flags = flags;
    fragment.header.set_last(true);
    
    fragment.data = data;
    
    return fragment;
}

}