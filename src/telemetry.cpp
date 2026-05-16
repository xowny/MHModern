#include "telemetry.h"

#include "logger.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace {

struct SourceTelemetry {
    const char* name = nullptr;
    mhmodern::telemetry::Stats stats{};
    std::array<mhmodern::telemetry::CallsiteStat, 8> callsites{};
    std::uint64_t last_value = 0;
    bool has_last_value = false;
};

CRITICAL_SECTION g_lock;
INIT_ONCE g_lock_init_once = INIT_ONCE_STATIC_INIT;
bool g_enabled = false;
std::uint32_t g_flush_interval_ms = 5000;
std::uint64_t g_last_flush_ms = 0;
std::array<SourceTelemetry, 2> g_sources{{
    {"timeGetTime", {}, 0, false},
    {"GetTickCount", {}, 0, false},
}};

BOOL CALLBACK initialize_lock_once(PINIT_ONCE, PVOID, PVOID*) {
    InitializeCriticalSection(&g_lock);
    return TRUE;
}

void ensure_initialized() {
    InitOnceExecuteOnce(&g_lock_init_once, initialize_lock_once, nullptr, nullptr);
}

SourceTelemetry* find_source(const char* source) {
    for (auto& entry : g_sources) {
        if (std::strcmp(entry.name, source) == 0) {
            return &entry;
        }
    }
    return nullptr;
}

const char* module_name_for_address(std::uintptr_t address, char* buffer, std::size_t buffer_size) {
    if (buffer == nullptr || buffer_size == 0 || address == 0) {
        return nullptr;
    }

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi)) == 0 ||
        mbi.AllocationBase == nullptr) {
        return nullptr;
    }

    buffer[0] = '\0';
    const DWORD copied = GetModuleFileNameA(
        reinterpret_cast<HMODULE>(mbi.AllocationBase),
        buffer,
        static_cast<DWORD>(buffer_size));
    if (copied == 0 || copied >= buffer_size) {
        return nullptr;
    }

    const auto basename = mhmodern::telemetry::detail::basename_from_path(buffer);
    if (basename.empty()) {
        return nullptr;
    }

    return basename.data();
}

void flush_locked(std::uint64_t now_ms) {
    if (!g_enabled) {
        return;
    }

    if (g_last_flush_ms != 0 &&
        now_ms >= g_last_flush_ms &&
        now_ms - g_last_flush_ms < g_flush_interval_ms) {
        return;
    }

    g_last_flush_ms = now_ms;
    for (auto& source : g_sources) {
        if (source.stats.count == 0) {
            continue;
        }

        mhmodern::logger::write(
            "TimingTelemetry[%s]: samples=%llu avg=%.2fms min=%ums max=%ums",
            source.name,
            static_cast<unsigned long long>(source.stats.count),
            mhmodern::telemetry::detail::average_delta_ms(source.stats),
            source.stats.min_delta_ms,
            source.stats.max_delta_ms);
        for (const auto& callsite : source.callsites) {
            if (callsite.count == 0) {
                continue;
            }
            char module_path[MAX_PATH] = {};
            const char* module_name = module_name_for_address(
                callsite.address,
                module_path,
                std::size(module_path));
            mhmodern::logger::write(
                "TimingTelemetry[%s][caller=%p module=%s]: calls=%llu",
                source.name,
                reinterpret_cast<const void*>(callsite.address),
                module_name != nullptr ? module_name : "unknown",
                static_cast<unsigned long long>(callsite.count));
        }
        source.stats = {};
        source.callsites = {};
    }
}

}  // namespace

namespace mhmodern::telemetry::detail {

void update_stats(Stats& stats, std::uint32_t delta_ms) {
    stats.total_delta_ms += delta_ms;
    stats.count += 1;
    if (stats.count == 1 || delta_ms < stats.min_delta_ms) {
        stats.min_delta_ms = delta_ms;
    }
    if (delta_ms > stats.max_delta_ms) {
        stats.max_delta_ms = delta_ms;
    }
}

double average_delta_ms(const Stats& stats) {
    if (stats.count == 0) {
        return 0.0;
    }
    return static_cast<double>(stats.total_delta_ms) / static_cast<double>(stats.count);
}

void update_callsite_stats(CallsiteStat* stats, std::size_t capacity, std::uintptr_t caller) {
    if (stats == nullptr || capacity == 0 || caller == 0) {
        return;
    }

    for (std::size_t index = 0; index < capacity; ++index) {
        if (stats[index].address == caller) {
            stats[index].count += 1;
            return;
        }
    }

    for (std::size_t index = 0; index < capacity; ++index) {
        if (stats[index].count == 0) {
            stats[index].address = caller;
            stats[index].count = 1;
            return;
        }
    }

    std::size_t smallest_index = 0;
    for (std::size_t index = 1; index < capacity; ++index) {
        if (stats[index].count < stats[smallest_index].count) {
            smallest_index = index;
        }
    }

    if (stats[smallest_index].count <= 1) {
        stats[smallest_index].address = caller;
        stats[smallest_index].count = 1;
    }
}

std::string_view basename_from_path(std::string_view path) {
    const auto separator = path.find_last_of("\\/");
    if (separator == std::string_view::npos) {
        return path;
    }
    return path.substr(separator + 1);
}

}  // namespace mhmodern::telemetry::detail

namespace mhmodern::telemetry {

void configure(bool enabled, std::uint32_t flush_interval_ms) {
    ensure_initialized();
    EnterCriticalSection(&g_lock);
    g_enabled = enabled;
    g_flush_interval_ms = flush_interval_ms == 0 ? 5000 : flush_interval_ms;
    g_last_flush_ms = 0;
    for (auto& source : g_sources) {
        source.stats = {};
        source.callsites = {};
        source.last_value = 0;
        source.has_last_value = false;
    }
    LeaveCriticalSection(&g_lock);
}

void record_timer_sample(const char* source, std::uint64_t current_ms, std::uintptr_t caller) {
    ensure_initialized();
    EnterCriticalSection(&g_lock);

    if (!g_enabled) {
        LeaveCriticalSection(&g_lock);
        return;
    }

    auto* telemetry = find_source(source);
    if (telemetry != nullptr) {
        if (telemetry->has_last_value && current_ms >= telemetry->last_value) {
            detail::update_stats(
                telemetry->stats,
                static_cast<std::uint32_t>(current_ms - telemetry->last_value));
        }
        telemetry->last_value = current_ms;
        telemetry->has_last_value = true;
        detail::update_callsite_stats(
            telemetry->callsites.data(),
            telemetry->callsites.size(),
            caller);
    }

    flush_locked(current_ms);
    LeaveCriticalSection(&g_lock);
}

}  // namespace mhmodern::telemetry
