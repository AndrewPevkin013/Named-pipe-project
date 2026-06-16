#include "../include/ipc_protocol.hpp"
#include <iostream>
#include <chrono>

namespace IPC {

bool MessageAssembler::add_fragment(const Fragment& fragment) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    MessageKey key {
        fragment.header.sender_id,
        fragment.header.message_id
    };

    auto it = assembly_map_.find(key);

    if (it == assembly_map_.end()) {
        AssemblyState state;
        state.message_id = fragment.header.message_id;
        state.total_size = fragment.header.total_size;
        state.total_fragments = fragment.header.total_fragments;
        state.message_checksum = fragment.header.message_checksum;
        state.fragments.resize(state.total_fragments);
        state.received.resize(state.total_fragments, false);
        state.created_at = std::chrono::steady_clock::now();
        
        it = assembly_map_.insert({key, state}).first;
    }

    if (fragment.header.total_fragments == 0) {
        std::cerr << "[Assembler] Invalid total_fragments" << std::endl;
        return false;
    }
    
    AssemblyState& state = it->second;
    
    if (fragment.header.total_fragments != state.total_fragments || fragment.header.total_size != state.total_size) {
        std::cerr << "[Assembler] Fragment mismatch for message " << fragment.header.message_id << std::endl;
        return false;
    }
    
    uint32_t index = fragment.header.fragment_index;

    if (index >= state.total_fragments) {
        std::cerr << "[Assembler] Invalid fragment index for message " << fragment.header.message_id << std::endl;
        return false;
    }

    if (state.received[index]) {
        return state.is_complete();
    }
    
    state.fragments[index] = fragment.data;
    state.received[index] = true;
    state.received_count++;
    
    
    return state.is_complete();
}

uint32_t crc32(const char* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < size; ++i) {
        crc ^= static_cast<unsigned char>(data[i]);

        for (int j = 0; j < 8; ++j) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

MessageAssembler::AssembledMessage MessageAssembler::get_assembled_message(uint16_t sender_id, uint32_t message_id) {
   std::lock_guard<std::mutex> lock(mutex_);

    AssembledMessage result;
    result.message_id = message_id;

    MessageKey key{ sender_id, message_id };

    auto it = assembly_map_.find(key);

    if (it == assembly_map_.end()) {
        return result;
    }

    AssemblyState& state = it->second;

    if (!state.is_complete()) {
        return result;
    }

    result.complete = true;

    result.data.reserve(state.total_size);

    for (uint32_t i = 0; i < state.total_fragments; ++i) {
        result.data.insert(
            result.data.end(),
            state.fragments[i].begin(),
            state.fragments[i].end()
        );
    }

    uint32_t expected_checksum = state.message_checksum;

    uint32_t actual_checksum = 0;

    if (!result.data.empty()) {
        actual_checksum =
            crc32(result.data.data(), result.data.size());
    }

    if (actual_checksum != expected_checksum) {
        std::cerr << "[Assembler] Message checksum mismatch\n";

        result.complete = false;

        assembly_map_.erase(it);

        return result;
    }

    assembly_map_.erase(it);

    return result;
}
}