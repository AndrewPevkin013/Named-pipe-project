#include "../include/ipc_sender.hpp"
#include "common/benchmark_utils.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <vector>

constexpr size_t MESSAGE_SIZE = 1024ull * 1024ull * 1024ull;
constexpr size_t FRAGMENT_SIZE = 1024ull * 1024ull;

int main() {
//     IPCSender sender(1);

//     auto data = bench::generate_data(MESSAGE_SIZE);

//     size_t mem_before = bench::memory_usage_bytes();
//     auto start = std::chrono::high_resolution_clock::now();

//     std::optional<uint64_t> message_id;

//     bool ok = sender.get_fragmenter().fragment_message_stream_with_size(
//         data,
//         FRAGMENT_SIZE,
//         [&](const IPC::FragmentView& view) -> bool {
//             if (!message_id) {
//                 message_id = view.header().message_id;
//             }
//             return sender.send_fragment(view);
//         }
//     );

//     if (ok && message_id) {
//         ok = sender.wait_for_ack(*message_id);
//     } else {
//         ok = false;
//     }

//     auto end = std::chrono::high_resolution_clock::now();
//     size_t mem_after = bench::memory_usage_bytes();

//     double ms = bench::ms_since(start, end);
//     double throughput = bench::mb_per_sec(MESSAGE_SIZE, ms);
//     double mem_delta_mb =
//         static_cast<double>(static_cast<long long>(mem_after) -
//                             static_cast<long long>(mem_before)) /
//         (1024.0 * 1024.0);

//     std::cout << "IPC large message benchmark\n";
//     std::cout << "Success: " << (ok ? "yes" : "no") << "\n";
//     std::cout << "Message size(MB): " << MESSAGE_SIZE / 1024 / 1024 << "\n";
//     std::cout << "Fragment size(KB): " << FRAGMENT_SIZE / 1024 << "\n";
//     std::cout << "Time(ms): " << ms << "\n";
//     std::cout << "Throughput(MB/s): " << throughput << "\n";
//     std::cout << "Memory delta(MB): " << mem_delta_mb << "\n";
// #ifdef _WIN32
//     system("pause");
// #endif
//     return ok ? 0 : 1;
}