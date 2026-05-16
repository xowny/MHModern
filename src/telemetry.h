#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mhmodern::telemetry {

struct CallsiteStat {
    std::uintptr_t address = 0;
    std::uint64_t count = 0;
};

struct Stats {
    std::uint64_t count = 0;
    std::uint64_t total_delta_ms = 0;
    std::uint32_t min_delta_ms = 0;
    std::uint32_t max_delta_ms = 0;
};

void configure(bool enabled, std::uint32_t flush_interval_ms);
void record_timer_sample(const char* source, std::uint64_t current_ms, std::uintptr_t caller = 0);

namespace detail {
void update_stats(Stats& stats, std::uint32_t delta_ms);
double average_delta_ms(const Stats& stats);
void update_callsite_stats(CallsiteStat* stats, std::size_t capacity, std::uintptr_t caller);
std::string_view basename_from_path(std::string_view path);
}

}  // namespace mhmodern::telemetry
