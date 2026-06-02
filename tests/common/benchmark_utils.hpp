#pragma once

#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace bench {

    inline std::vector<char> generate_data(size_t size, char seed = 'x') {
        std::vector<char> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<char>(seed + (i % 23));
        }
        return data;
    }

    inline uint64_t fnv1a_hash(const std::vector<char>& data) {
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : data) {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    inline size_t memory_usage_bytes() {
    #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
            sizeof(pmc))) {
            return static_cast<size_t>(pmc.WorkingSetSize);
        }
        return 0;
    #else
        struct rusage usage {};
        getrusage(RUSAGE_SELF, &usage);
        return static_cast<size_t>(usage.ru_maxrss) * 1024;
    #endif
    }

    inline double ms_since(std::chrono::high_resolution_clock::time_point start,
                        std::chrono::high_resolution_clock::time_point end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    inline double mb_per_sec(size_t bytes, double ms) {
        double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        return mb / (ms / 1000.0);
    }

}