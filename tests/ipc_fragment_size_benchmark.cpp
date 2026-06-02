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

constexpr std::size_t MESSAGE_SIZE = 1024ULL * 1024ULL * 1024ULL;

const std::vector<std::size_t> FRAGMENT_SIZES = {
    16ULL * 1024ULL,
    32ULL * 1024ULL,
    64ULL * 1024ULL,
    128ULL * 1024ULL,
    256ULL * 1024ULL,
    512ULL * 1024ULL,
    1024ULL * 1024ULL
};

struct BenchmarkResult {
    std::size_t body_size;
    std::size_t full_size;
    std::size_t fragment_size;
    std::size_t fragments_count;
    bool success;
    bool ack_received;
    double time_ms;
    double throughput_mbps;
};

std::string format_size(std::size_t bytes) {
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

BenchmarkResult run_case(const std::vector<char>& full_message,
                         std::size_t body_size,
                         std::size_t fragment_size) {
    IPCSender sender;

    std::optional<uint64_t> message_id;
    std::size_t fragments_count = 0;

    auto start = std::chrono::high_resolution_clock::now();

    bool stream_success = sender.get_fragmenter().fragment_message_stream_with_size(
        full_message,
        fragment_size,
        [&](const IPC::FragmentView& view) -> bool {
            if (!message_id) {
                message_id = view.header().message_id;
                fragments_count = view.header().total_fragments;
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

    double time_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    double seconds = time_ms / 1000.0;
    double mb = static_cast<double>(full_message.size()) / (1024.0 * 1024.0);
    double throughput = seconds > 0.0 ? mb / seconds : 0.0;

    return {
        body_size,
        full_message.size(),
        fragment_size,
        fragments_count,
        stream_success && ack_received,
        ack_received,
        time_ms,
        throughput
    };
}

int main() {
    std::cout << "IPC fragment size benchmark\n\n";
    std::cout << "Before running this test, start test_receiver.exe\n\n";

    std::cout << "Generating 1 GB message...\n";
    std::string body = make_message(MESSAGE_SIZE);
    std::vector<char> full_message = build_full_message(body);

    std::cout << "Body size: " << format_size(body.size()) << "\n";
    std::cout << "Full message size: " << full_message.size() << " bytes\n\n";

    std::vector<BenchmarkResult> results;
    results.reserve(FRAGMENT_SIZES.size());

    for (std::size_t fragment_size : FRAGMENT_SIZES) {
        std::cout << "Running fragment size: "
                  << format_size(fragment_size) << "\n";

        BenchmarkResult result = run_case(
            full_message,
            body.size(),
            fragment_size
        );

        results.push_back(result);

        std::cout << "Fragments: " << result.fragments_count << "\n";
        std::cout << "ACK: " << (result.ack_received ? "yes" : "no") << "\n";
        std::cout << "Result: " << (result.success ? "success" : "failed") << "\n";
        std::cout << "Time(ms): " << std::fixed << std::setprecision(2)
                  << result.time_ms << "\n";
        std::cout << "Throughput(MB/s): " << std::fixed << std::setprecision(2)
                  << result.throughput_mbps << "\n\n";
    }

    std::cout << std::left
              << std::setw(14) << "FragSize"
              << std::setw(14) << "Fragments"
              << std::setw(10) << "ACK"
              << std::setw(10) << "Result"
              << std::setw(14) << "Time(ms)"
              << std::setw(18) << "Throughput(MB/s)"
              << "\n";

    std::cout << std::string(80, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left
                  << std::setw(14) << format_size(r.fragment_size)
                  << std::setw(14) << r.fragments_count
                  << std::setw(10) << (r.ack_received ? "yes" : "no")
                  << std::setw(10) << (r.success ? "success" : "failed")
                  << std::setw(14) << std::fixed << std::setprecision(2) << r.time_ms
                  << std::setw(18) << std::fixed << std::setprecision(2) << r.throughput_mbps
                  << "\n";
    }

#ifdef _WIN32
    system("pause");
#endif

    return 0;
}