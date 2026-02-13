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
        state.fragments.resize(state.total_fragments);
        state.received.resize(state.total_fragments, false);
        state.created_at = std::chrono::steady_clock::now();
        
        it = assembly_map_.insert({key, state}).first;
    }
    
    AssemblyState& state = it->second;
    
    if (fragment.header.total_fragments != state.total_fragments || fragment.header.total_size != state.total_size) {
        std::cerr << "[Assembler] Fragment mismatch for message " << fragment.header.message_id << std::endl;
        return false;
    }
    
    uint32_t index = fragment.header.fragment_index;
    if (state.received[index]) {
        return state.is_complete();
    }
    
    state.fragments[index] = fragment.data;
    state.received[index] = true;
    state.received_count++;
    
    if (state.received_count % 10 == 0 || state.is_complete()) {
        std::cout << "[Assembler] Message " << fragment.header.message_id << ": " << state.received_count << "/" << state.total_fragments << " fragments received" << std::endl;
    }
    
    return state.is_complete();
}

MessageAssembler::AssembledMessage MessageAssembler::get_assembled_message(uint32_t sender_id, uint64_t message_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AssembledMessage result;
    result.message_id = message_id;
    
    MessageKey key { sender_id, message_id };
    auto it = assembly_map_.find(key);

    if (it == assembly_map_.end()) {
        return result;
    }
    
    AssemblyState& state = it->second;
    result.complete = state.is_complete();
    
    if (result.complete) {
        result.data.reserve(state.total_size);
        
        for (uint32_t i = 0; i < state.total_fragments; i++) {
            result.data.insert(result.data.end(), state.fragments[i].begin(), state.fragments[i].end());
        }
        
        std::cout << "[Assembler] Message " << message_id << " assembled successfully (" << result.data.size() << " bytes)" << std::endl;
        assembly_map_.erase(it);
    }
    
    return result;
}
}