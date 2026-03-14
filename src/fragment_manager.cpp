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
        Fragment f;
        fill_fragment(f, message_id, total_size, fragment_size, i, total_fragments, message_data);
        fragments.push_back(std::move(f));
    }

    return fragments;
}

bool MessageFragmenter::fragment_message_stream(const std::vector<char>& message_data, FragmentCallback callback) {
    size_t fragment_size = calculate_fragment_size(message_data.size());
    return fragment_message_stream_with_size(message_data, fragment_size, std::move(callback));
}

bool MessageFragmenter::fragment_message_stream_with_size(const std::vector<char>& message_data, size_t fragment_size, FragmentCallback callback) {
    fragment_size = std::clamp(fragment_size, MIN_FRAGMENT_SIZE, MAX_FRAGMENT_SIZE);

    uint64_t message_id = next_message_id_++;
    size_t total_size = message_data.size();
    size_t total_fragments = (total_size + fragment_size - 1) / fragment_size;
    
    FragmentHeader base_header;
    base_header.sender_id = 0;
    base_header.message_id = message_id;
    base_header.total_size = static_cast<uint32_t>(total_size);
    base_header.fragment_size = 0; 
    base_header.fragment_index = 0;
    base_header.total_fragments = static_cast<uint32_t>(total_fragments);
    base_header.flags = FLAG_DATA;
    
    for (size_t i = 0; i < total_fragments; ++i) {
        size_t offset = i * fragment_size;
        size_t size = std::min(fragment_size, total_size - offset);
        
        FragmentHeader header = base_header;
        header.fragment_size = static_cast<uint32_t>(size);
        header.fragment_index = static_cast<uint32_t>(i);
        header.flags = (i + 1 == total_fragments) ? FLAG_LAST : FLAG_DATA;
        
        FragmentView view(header, message_data.data() + offset, size);
        
        if (!callback(view)) {
            return false;
        }
    }
    
    return true;
}


void MessageFragmenter::fill_fragment(Fragment& f, uint64_t message_id, size_t total_size, size_t fragment_size, size_t fragment_index, size_t total_fragments, const std::vector<char>& message_data) {
    size_t offset = fragment_index * fragment_size;
    size_t size = std::min(fragment_size, total_size - offset);
    
    f.header.message_id = message_id;
    f.header.total_size = static_cast<uint32_t>(total_size);
    f.header.fragment_size = static_cast<uint32_t>(size);
    f.header.fragment_index = static_cast<uint32_t>(fragment_index);
    f.header.total_fragments = static_cast<uint32_t>(total_fragments);
    f.header.flags = (fragment_index + 1 == total_fragments) ? FLAG_LAST : FLAG_DATA;
    
    f.data.clear();
    f.data.insert(f.data.end(), message_data.begin() + offset, message_data.begin() + offset + size);
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