#include "qpc_timer.h"

#include <atomic>

namespace {

std::atomic<int64_t> g_frequency = 0;
std::atomic<int64_t> g_start_counter = 0;

}  // namespace

namespace mhmodern::time_math {

uint64_t counter_to_milliseconds(int64_t counter_delta, int64_t frequency) {
    if (counter_delta <= 0 || frequency <= 0) {
        return 0;
    }

    return static_cast<uint64_t>((counter_delta * 1000) / frequency);
}

uint32_t wrap_milliseconds(uint64_t milliseconds) {
    return static_cast<uint32_t>(milliseconds & 0xffffffffull);
}

uint32_t lower_32_bits(uint64_t value) {
    return static_cast<uint32_t>(value & 0xffffffffull);
}

uint32_t upper_32_bits(uint64_t value) {
    return static_cast<uint32_t>((value >> 32) & 0xffffffffull);
}

}  // namespace mhmodern::time_math

namespace mhmodern::qpc_timer {

uint64_t frequency() {
    const auto current_frequency = g_frequency.load();
    return current_frequency > 0 ? static_cast<uint64_t>(current_frequency) : 0ull;
}

uint64_t raw_counter() {
    LARGE_INTEGER current{};
    if (!QueryPerformanceCounter(&current)) {
        return GetTickCount64();
    }

    return static_cast<uint64_t>(current.QuadPart);
}

bool initialize() {
    LARGE_INTEGER frequency{};
    LARGE_INTEGER counter{};
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&counter)) {
        return false;
    }

    g_frequency.store(frequency.QuadPart);
    g_start_counter.store(counter.QuadPart);
    return true;
}

uint64_t now_ms_64() {
    const int64_t frequency = g_frequency.load();
    const int64_t start_counter = g_start_counter.load();
    const uint64_t current = raw_counter();
    if (frequency <= 0 || current < static_cast<uint64_t>(start_counter)) {
        return GetTickCount64();
    }

    return time_math::counter_to_milliseconds(
        static_cast<int64_t>(current - static_cast<uint64_t>(start_counter)),
        frequency);
}

uint32_t now_ms_32() {
    return time_math::wrap_milliseconds(now_ms_64());
}

}  // namespace mhmodern::qpc_timer
