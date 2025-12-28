#include "../include/ipc_protocol.hpp"
#include <algorithm>
#include <iostream>
#include <atomic>

namespace IPC {

size_t MessageFragmenter::calculate_optimal_fragment_size(size_t total_message_size) const {
    
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

std::vector<Fragment> MessageFragmenter::fragment_message(
    const std::vector<char>& message_data) {
    size_t fragment_size = calculate_optimal_fragment_size(message_data.size());
    
    return fragment_message_with_size(message_data, fragment_size);
}

std::vector<Fragment> MessageFragmenter::fragment_message_with_size(
    const std::vector<char>& message_data,
    size_t fragment_size) {
    
    if (fragment_size < MIN_FRAGMENT_SIZE) {
        fragment_size = MIN_FRAGMENT_SIZE;
    }
    if (fragment_size > MAX_FRAGMENT_SIZE) {
        fragment_size = MAX_FRAGMENT_SIZE;
    }
    
    std::vector<Fragment> fragments;
    
    uint64_t message_id = next_message_id_++;
    uint32_t total_size = static_cast<uint32_t>(message_data.size());
    
    uint32_t total_fragments = static_cast<uint32_t>(
        (total_size + fragment_size - 1) / fragment_size);
    
    size_t offset = 0;
    
    for (uint32_t i = 0; i < total_fragments; i++) {
        Fragment fragment;
        
        fragment.header.message_id = message_id;
        fragment.header.total_size = total_size;
        fragment.header.fragment_index = i;
        fragment.header.total_fragments = total_fragments;
        fragment.header.set_last(i == total_fragments - 1);
        
        size_t current_fragment_size = std::min(
            fragment_size, 
            message_data.size() - offset);
        
        fragment.header.fragment_size = static_cast<uint32_t>(current_fragment_size);
        
        fragment.data.resize(current_fragment_size);
        std::copy_n(
            message_data.begin() + offset,
            current_fragment_size,
            fragment.data.begin()
        );
        
        fragments.push_back(std::move(fragment));
        offset += current_fragment_size;
    }
    
    std::cout << "[Fragmenter] Message " << message_id 
              << " fragmented into " << total_fragments 
              << " fragments (optimal size: " << fragment_size 
              << " bytes, total: " << total_size << " bytes)" << std::endl;
    
    return fragments;
}

Fragment MessageFragmenter::create_control_message(
    const std::vector<char>& data,
    uint8_t flags) {
    
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