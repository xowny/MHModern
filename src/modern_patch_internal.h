#pragma once

#include <filesystem>
#include <cstdint>
#include <string>

namespace mhmodern::modern_patch::detail {

struct Settings {
    bool miles_audio_patch = true;
    bool log_audio_init = false;
    bool detect_miles_primary_format = true;
    bool miles_driver_fallback = true;
    bool raw_audio_buffered_open = true;
    bool event_patch = false;
    bool pool_audio_temp_events = false;
    bool log_event_init = false;
    bool heap_patch = false;
    bool heap_terminate_on_corruption = true;
    bool heap_low_fragmentation = true;
    bool input_patch = true;
    bool input_auto_reacquire = true;
    bool log_input_init = false;
    bool version_patch = true;
    bool directx_probe_patch = true;
    bool system_parameters_patch = true;
    bool error_mode_patch = true;
    bool qpc_timer_hooks = true;
    bool qpc_rdtsc_hook = true;
    bool timing_telemetry = false;
    bool crash_dumps = true;
    bool full_memory_crash_dumps = false;
    bool dpi_awareness = true;
    bool scheduler_precision = true;
    std::uint32_t miles_fallback_rate = 48000;
    std::int32_t miles_fallback_bits = 16;
    std::int32_t miles_fallback_channels = 2;
    std::uint32_t telemetry_flush_interval_ms = 5000;
    std::string crash_dump_directory = "MHModern_crashes";
};

Settings load_settings_from(const std::filesystem::path& ini_path);
Settings load_settings();
bool enable_dpi_awareness();
unsigned tighten_scheduler_precision();

}  // namespace mhmodern::modern_patch::detail
