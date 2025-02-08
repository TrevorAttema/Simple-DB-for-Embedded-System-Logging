// Instrumentation.h
#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdio>

// Structure to hold timing information for one function.
struct FunctionStats {
    unsigned int count = 0;
    double totalTimeMs = 0.0;
};

// Global registry for instrumentation data.
inline std::unordered_map<std::string, FunctionStats> gFunctionStats;
inline std::mutex gStatsMutex;

// RAII class that records function execution time.
class ScopedTimer {
public:
    // Start the timer with the given function name.
    ScopedTimer(const std::string& funcName)
        : m_funcName(funcName),
        m_start(std::chrono::high_resolution_clock::now()) {
    }

    // On destruction, update the global stats.
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        double elapsedMs =
            std::chrono::duration<double, std::milli>(end - m_start).count();
        std::lock_guard<std::mutex> lock(gStatsMutex);
        auto& stats = gFunctionStats[m_funcName];
        stats.count++;
        stats.totalTimeMs += elapsedMs;
    }
private:
    std::string m_funcName;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

// Utility function to print a timing report.
inline void PrintInstrumentationReport() {
    std::lock_guard<std::mutex> lock(gStatsMutex);

    if (gFunctionStats.empty()) {
        std::printf("Instrumentation report: No stats collected.\n");
        return;
    }

    std::printf("=== Instrumentation Timing Report ===\n");
    std::printf("%-40s %10s %15s %15s\n", "Function", "Calls", "Total Time (ms)", "Avg Time (ms)");

    for (const auto& pair : gFunctionStats) {
        const std::string& name = pair.first;
        const FunctionStats& stats = pair.second;
        double avg = stats.count ? stats.totalTimeMs / stats.count : 0.0;
        std::printf("%-40s %10u %15.3f %15.3f\n", name.c_str(), stats.count, stats.totalTimeMs, avg);
    }

    std::printf("=====================================\n");
}

