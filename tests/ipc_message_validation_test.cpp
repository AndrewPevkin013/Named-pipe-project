#define _CRT_SECURE_NO_WARNINGS

#include "../include/ipc_sender.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

struct TestCase {
    std::string name;
    std::size_t body_size_bytes;
    std::size_t fragment_size;
};

struct TestResult {
    std::string name;
    std::size_t body_size_bytes;
    std::size_t full_size_bytes;
    std::size_t requested_fragment_size;
    std::size_t actual_first_fragment_size;
    std::size_t actual_fragments_count;
    bool stream_success;
    bool ack_received;
    double time_ms;
};

std::string format_size(std::size_t bytes) {
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        return std::to_string(bytes / (1024ULL * 1024ULL * 1024ULL)) + " GB";
    }

    if (bytes >= 1024ULL * 1024ULL) {
        return std::to_string(bytes / (1024ULL * 1024ULL)) + " MB";
    }

    if (bytes >= 1024ULL) {
        return std::to_string(bytes / 1024ULL) + " KB";
    }

    return std::to_string(bytes) + " B";
}

std::string make_message(std::size_t size) {
    std::string msg(size, '\0');

    for (std::size_t i = 0; i < size; ++i) {
        msg[i] = static_cast<char>('A' + (i % 26));
    }

    return msg;
}

std::vector<char> build_full_message(const std::string& body) {
    std::ostringstream meta;
    meta << "type:text\n"
         << "size:" << body.size() << "\n"
         << "timestamp:test\n";

    std::string metadata = meta.str();

    std::vector<char> full_message;
    full_message.reserve(metadata.size() + body.size());

    full_message.insert(full_message.end(), metadata.begin(), metadata.end());
    full_message.insert(full_message.end(), body.begin(), body.end());

    return full_message;
}

TestResult run_case(const TestCase& tc) {
    IPCSender sender;

    std::string body = make_message(tc.body_size_bytes);
    std::vector<char> full_message = build_full_message(body);

    std::optional<uint64_t> message_id;
    std::size_t actual_fragments_count = 0;
    std::size_t actual_first_fragment_size = 0;

    auto start = std::chrono::high_resolution_clock::now();

    bool stream_success = sender.get_fragmenter().fragment_message_stream_with_size(
        full_message,
        tc.fragment_size,
        [&](const IPC::FragmentView& view) -> bool {
            if (!message_id) {
                message_id = view.header().message_id;
                actual_fragments_count = view.header().total_fragments;
                actual_first_fragment_size = view.header().fragment_size;
            }

            return sender.send_fragment(view);
        }
    );

    bool ack_received = false;

    if (stream_success && message_id) {
        ack_received = sender.wait_for_ack(*message_id);
    }

    auto end = std::chrono::high_resolution_clock::now();

    sender.disconnect();

    double elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    return {
        tc.name,
        tc.body_size_bytes,
        full_message.size(),
        tc.fragment_size,
        actual_first_fragment_size,
        actual_fragments_count,
        stream_success,
        ack_received,
        elapsed_ms
    };
}

int main() {
//     std::vector<TestCase> cases = {
//         { "Small message", 1ULL * 1024ULL, 1ULL * 1024ULL },
//         { "Medium message", 200ULL * 1024ULL, 16ULL * 1024ULL },
//         { "Large message", 1024ULL * 1024ULL * 1024ULL, 1024ULL * 1024ULL }
//         // Можно заменить последний fragment_size на:
//         // 50ULL * 1024ULL * 1024ULL
//     };

//     std::vector<TestResult> results;
//     results.reserve(cases.size());

//     std::cout << "IPC message validation test\n\n";
//     std::cout << "Before running this test, start test_receiver.exe\n\n";

//     for (const auto& tc : cases) {
//         TestResult result = run_case(tc);
//         results.push_back(result);

//         std::cout << "[" << result.name << "]\n";
//         std::cout << "Body size: " << format_size(result.body_size_bytes) << "\n";
//         std::cout << "Full message size: " << result.full_size_bytes << " bytes\n";
//         std::cout << "Requested fragment size: "
//                   << format_size(result.requested_fragment_size) << "\n";
//         std::cout << "Actual first fragment size: "
//                   << format_size(result.actual_first_fragment_size) << "\n";
//         std::cout << "Actual fragments: " << result.actual_fragments_count << "\n";
//         std::cout << "Stream success: "
//                   << (result.stream_success ? "yes" : "no") << "\n";
//         std::cout << "ACK received: "
//                   << (result.ack_received ? "yes" : "no") << "\n";
//         std::cout << "Result: "
//                   << ((result.stream_success && result.ack_received) ? "success" : "failed") << "\n";
//         std::cout << "Time(ms): " << std::fixed << std::setprecision(2)
//                   << result.time_ms << "\n\n";
//     }

//     std::cout << std::left
//               << std::setw(18) << "Case"
//               << std::setw(12) << "Body"
//               << std::setw(12) << "FragSize"
//               << std::setw(12) << "Fragments"
//               << std::setw(10) << "ACK"
//               << std::setw(10) << "Result"
//               << std::setw(12) << "Time(ms)"
//               << "\n";

//     std::cout << std::string(86, '-') << "\n";

//     for (const auto& r : results) {
//         bool success = r.stream_success && r.ack_received;

//         std::cout << std::left
//                   << std::setw(18) << r.name
//                   << std::setw(12) << format_size(r.body_size_bytes)
//                   << std::setw(12) << format_size(r.requested_fragment_size)
//                   << std::setw(12) << r.actual_fragments_count
//                   << std::setw(10) << (r.ack_received ? "yes" : "no")
//                   << std::setw(10) << (success ? "success" : "failed")
//                   << std::setw(12) << std::fixed << std::setprecision(2) << r.time_ms
//                   << "\n";
//     }

// #ifdef _WIN32
//     system("pause");
// #endif

    return 0;
}