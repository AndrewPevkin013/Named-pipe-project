#define _CRT_SECURE_NO_WARNINGS
#include "../include/ipc_sender.hpp"
#include "test_config.hpp"

#include <thread>
#include <random>
#include <vector>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iostream>
#include <numeric>
#include <iomanip>
#include <cstring>
#include <optional>
#include <sstream> 

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#endif

using namespace TestConfig;

const int REPETITIONS = 3;

const std::vector<size_t> FRAGMENT_SIZES = {
    16 * 1024,
    32 * 1024,
    64 * 1024,
    128 * 1024,
    256 * 1024,
    512 * 1024,
    1024 * 1024,
    5 * 1024 * 1024,
    10 * 1024 * 1024,
    50 * 1024 * 1024
};

const std::vector<size_t> MESSAGE_SIZES = {
    10,
    1024,
    200 * 1024,
    1024 * 1024 * 1024
};

const std::vector<int> MESSAGE_COUNTS = {1, 5};

// Замеры памяти
#ifdef _WIN32
size_t get_memory_usage() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}
#endif

// Baseline memcpy (скорость копирования в памяти)
double run_memcpy_test(size_t size_mb) {
    size_t bytes = size_mb * 1024 * 1024;
    std::vector<char> src(bytes, 'x');
    std::vector<char> dst(bytes, 'y');

    auto start = std::chrono::high_resolution_clock::now();
    memcpy(dst.data(), src.data(), bytes);
    auto end = std::chrono::high_resolution_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();
    return size_mb / seconds; // MB/s
}

std::string generate_fixed_message(size_t size_bytes) {
    thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> char_dist(32, 126);

    std::string msg;
    msg.resize(size_bytes);
    for (size_t i = 0; i < size_bytes; ++i)
        msg[i] = static_cast<char>(char_dist(rng));
    return msg;
}

struct DetailedRunResult {
    int msg_count;
    size_t msg_size_bytes;
    size_t fragment_size;
    bool all_success;
    
    double total_time_ms; 
    double total_syscall_time_ms;
    double total_data_mb;
    double throughput_mbps;
    double avg_latency_ms;
    double memory_delta_mb;
};

int main() {
    std::filesystem::create_directories(LOG_DIR);

    std::ofstream log(CLIENT_LOG_FILE, std::ios::out | std::ios::trunc);
    if (!log) {
        std::cerr << "Failed to open client log\n";
        return 1;
    }

    std::cout << "Baseline: memcpy 1 GB\n";
    double memcpy_speed = run_memcpy_test(1024);
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) << memcpy_speed << " MB/s\n";

    std::cout << "Extended performance analysis\n";
    std::cout << std::left
              << std::setw(8)  << "MsgSize"
              << std::setw(6)  << "MsgCnt"
              << std::setw(12) << "FragSize"
              << std::setw(8)  << "Success"
              << std::setw(12) << "TotTime(ms)"
              << std::setw(14) << "Throughput"
              << std::setw(14) << "AvgLat(ms)"
              << std::setw(16) << "SyscallTime(ms)"
              << std::setw(14) << "Overhead(ms)"
              << std::setw(12) << "MemDelta(MB)" << "\n";
    std::cout << std::string(120, '-') << "\n";

    for (size_t msg_size : MESSAGE_SIZES) {
        for (int msg_cnt : MESSAGE_COUNTS) {
            for (size_t fsize : FRAGMENT_SIZES) {
                // Пропускаем нерелевантные конфигурации (фрагмент > сообщения для мелких сообщений)
                if (fsize > msg_size && msg_size < 1024 * 1024) continue;

                std::vector<DetailedRunResult> runs;

                for (int rep = 0; rep < REPETITIONS; ++rep) {
                    // Замер памяти до
                    size_t mem_before = get_memory_usage();

                    IPCSender sender;
                    bool all_ok = true;
                    double total_syscall_time = 0.0;
                    size_t total_bytes = 0;
                    std::vector<long long> latencies_us;

                    auto test_start = std::chrono::high_resolution_clock::now();

                    for (int m = 0; m < msg_cnt; ++m) {
                        std::string msg = generate_fixed_message(msg_size);

                        std::vector<char> body(msg.begin(), msg.end());
                        auto now = std::chrono::system_clock::now();
                        auto time_t_now = std::chrono::system_clock::to_time_t(now);
                        std::tm tm_now = *std::localtime(&time_t_now);

                        std::ostringstream meta;
                        meta << "type:text\n"
                             << "size:" << body.size() << "\n"
                             << "timestamp:" << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S") << "\n";

                        std::string metadata = meta.str();
                        std::vector<char> full_message;
                        full_message.insert(full_message.end(), metadata.begin(), metadata.end());
                        full_message.insert(full_message.end(), body.begin(), body.end());

                        std::optional<uint64_t> message_id;
                        double msg_syscall_time = 0.0;

                        auto t1 = std::chrono::high_resolution_clock::now();

                        bool ok = sender.get_fragmenter().fragment_message_stream_with_size(
                            full_message,
                            fsize,
                            [&](const IPC::FragmentView& view) -> bool {
                                if (!message_id) message_id = view.header().message_id;
                                
                                auto sys_start = std::chrono::high_resolution_clock::now();
                                bool res = sender.send_fragment(view);
                                auto sys_end = std::chrono::high_resolution_clock::now();
                                
                                msg_syscall_time += std::chrono::duration<double, std::milli>(sys_end - sys_start).count();
                                return res;
                            });

                        auto t2 = std::chrono::high_resolution_clock::now();

                        if (ok && message_id) {
                            bool ack_ok = sender.wait_for_ack(*message_id);
                            if (!ack_ok) ok = false;
                        } else {
                            ok = false;
                        }

                        if (ok) {
                            total_bytes += full_message.size();
                            long long latency_us = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                            latencies_us.push_back(latency_us);
                            total_syscall_time += msg_syscall_time;
                        } else {
                            all_ok = false;
                            log << "Failed: msg=" << m << " fragSize=" << fsize/1024 << "KB rep=" << rep << "\n";
                        }
                    }

                    auto test_end = std::chrono::high_resolution_clock::now();

                    // Замер памяти после
                    size_t mem_after = get_memory_usage();

                    if (all_ok && !latencies_us.empty()) {
                        double elapsed_ms = std::chrono::duration<double, std::milli>(test_end - test_start).count();
                        double total_data_mb = static_cast<double>(total_bytes) / (1024.0 * 1024.0);
                        double throughput = total_data_mb / (elapsed_ms / 1000.0);
                        double avg_latency_ms = std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0) / latencies_us.size() / 1000.0;
                        long long delta = static_cast<long long>(mem_after) - static_cast<long long>(mem_before);
                        double mem_delta_mb = delta / (1024.0 * 1024.0);

                        runs.push_back({
                            msg_cnt,
                            msg_size,
                            fsize,
                            true,
                            elapsed_ms,
                            total_syscall_time,
                            total_data_mb,
                            throughput,
                            avg_latency_ms,
                            mem_delta_mb
                        });
                    } else {
                        runs.push_back({msg_cnt, msg_size, fsize, false, 0,0,0,0,0,0});
                    }
                }

                int valid = 0;
                double sum_time=0, sum_sys=0, sum_thr=0, sum_lat=0, sum_mem=0;
                for (const auto& r : runs) {
                    if (r.all_success) {
                        valid++;
                        sum_time += r.total_time_ms;
                        sum_sys += r.total_syscall_time_ms;
                        sum_thr += r.throughput_mbps;
                        sum_lat += r.avg_latency_ms;
                        sum_mem += r.memory_delta_mb;
                    }
                }

                if (valid > 0) {
                    double avg_time = sum_time / valid;
                    double avg_sys = sum_sys / valid;
                    double avg_thr = sum_thr / valid;
                    double avg_lat = sum_lat / valid;
                    double avg_mem = sum_mem / valid;
                    double avg_overhead = avg_time - avg_sys;

                    std::string msg_size_str;
                    if (msg_size < 1024) {
                        msg_size_str = std::to_string(msg_size) + "B";
                    } else if (msg_size < 1024 * 1024) {
                        msg_size_str = std::to_string(msg_size / 1024) + "KB";
                    } else {
                        msg_size_str = std::to_string(msg_size / (1024 * 1024)) + "MB";
                    }

                    std::cout << std::left
                              << std::setw(8)  << msg_size_str
                              << std::setw(6)  << msg_cnt
                              << std::setw(12) << (std::to_string(fsize/1024) + "KB")
                              << std::setw(8)  << (std::to_string(valid) + "/" + std::to_string(REPETITIONS))
                              << std::fixed << std::setprecision(2)
                              << std::setw(12) << avg_time
                              << std::setw(14) << avg_thr
                              << std::setw(14) << avg_lat
                              << std::setw(16) << avg_sys
                              << std::setw(14) << avg_overhead
                              << std::setw(12) << avg_mem << "\n";
                } else {
                    std::string msg_size_str = (msg_size < 1024) ? std::to_string(msg_size) + "B" :
                                               (msg_size < 1024*1024) ? std::to_string(msg_size/1024) + "KB" :
                                               std::to_string(msg_size/(1024*1024)) + "MB";
                    std::cout << std::left
                              << std::setw(8)  << msg_size_str
                              << std::setw(6)  << msg_cnt
                              << std::setw(12) << (std::to_string(fsize/1024) + "KB")
                              << "   failed\n";
                }
            }
            std::cout << "\n";
        }
    }

#ifdef _WIN32
    system("pause");
#endif
    return 0;
}