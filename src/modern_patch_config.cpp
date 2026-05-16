#include "modern_patch_internal.h"

#include <windows.h>
#include <mmsystem.h>
#include <shellscalingapi.h>

#include <filesystem>

namespace {

using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(PROCESS_DPI_AWARENESS);
using TimeBeginPeriodFn = MMRESULT(WINAPI*)(UINT);
using SetProcessInformationFn = BOOL(WINAPI*)(HANDLE, int, LPVOID, DWORD);

constexpr ULONG kProcessPowerThrottlingCurrentVersion = 1;
constexpr ULONG kProcessPowerThrottlingExecutionSpeed = 0x1;
constexpr int kProcessPowerThrottlingInformationClass = 4;

struct PowerThrottlingStateCompat {
    ULONG Version;
    ULONG ControlMask;
    ULONG StateMask;
};

std::filesystem::path exe_directory() {
    char buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) {
        return std::filesystem::current_path();
    }

    return std::filesystem::path(buffer).parent_path();
}

bool read_ini_bool(const char* section, const char* key, bool default_value, const std::filesystem::path& ini_path) {
    return GetPrivateProfileIntA(
               section,
               key,
               default_value ? 1 : 0,
               ini_path.string().c_str()) != 0;
}

std::uint32_t read_ini_u32_in_range(
    const char* section,
    const char* key,
    std::uint32_t default_value,
    std::uint32_t min_value,
    std::uint32_t max_value,
    const std::filesystem::path& ini_path) {
    const UINT raw = GetPrivateProfileIntA(
        section,
        key,
        static_cast<int>(default_value),
        ini_path.string().c_str());
    if (raw < min_value || raw > max_value) {
        return default_value;
    }

    return raw;
}

std::int32_t read_ini_audio_bits(const std::filesystem::path& ini_path) {
    const auto raw = static_cast<std::int32_t>(
        GetPrivateProfileIntA("Main", "MilesFallbackBits", 16, ini_path.string().c_str()));
    switch (raw) {
    case 8:
    case 16:
    case 24:
    case 32:
        return raw;
    default:
        return 16;
    }
}

std::int32_t read_ini_audio_channels(const std::filesystem::path& ini_path) {
    const auto raw = static_cast<std::int32_t>(
        GetPrivateProfileIntA("Main", "MilesFallbackChannels", 2, ini_path.string().c_str()));
    return raw == 1 ? 1 : (raw == 2 ? 2 : 2);
}

}  // namespace

namespace mhmodern::modern_patch::detail {

Settings load_settings_from(const std::filesystem::path& ini_path) {
    Settings settings{};
    settings.miles_audio_patch = read_ini_bool("Main", "EnableMilesAudioPatch", true, ini_path);
    settings.log_audio_init = read_ini_bool("Main", "LogMilesAudioInit", false, ini_path);
    settings.detect_miles_primary_format = read_ini_bool("Main", "DetectMilesPrimaryFormat", true, ini_path);
    settings.miles_driver_fallback = read_ini_bool("Main", "EnableMilesDriverFallback", true, ini_path);
    settings.raw_audio_buffered_open = read_ini_bool("Main", "EnableRawAudioBufferedOpen", true, ini_path);
    settings.event_patch = read_ini_bool("Main", "EnableEventPatch", false, ini_path);
    settings.pool_audio_temp_events = read_ini_bool("Main", "PoolAudioTempEvents", false, ini_path);
    settings.log_event_init = read_ini_bool("Main", "LogEventPatchInit", false, ini_path);
    settings.heap_patch = read_ini_bool("Main", "EnableHeapPatch", false, ini_path);
    settings.heap_terminate_on_corruption =
        read_ini_bool("Main", "EnableHeapTerminateOnCorruption", true, ini_path);
    settings.heap_low_fragmentation = read_ini_bool("Main", "EnableLowFragmentationHeap", true, ini_path);
    settings.input_patch = read_ini_bool("Main", "EnableInputPatch", true, ini_path);
    settings.input_auto_reacquire = read_ini_bool("Main", "EnableInputAutoReacquire", true, ini_path);
    settings.raw_mouse_input = read_ini_bool("Main", "EnableRawMouseInput", true, ini_path);
    settings.log_input_init = read_ini_bool("Main", "LogInputInit", false, ini_path);
    settings.version_patch = read_ini_bool("Main", "EnableVersionPatch", true, ini_path);
    settings.directx_probe_patch = read_ini_bool("Main", "EnableDirectXProbePatch", true, ini_path);
    settings.system_parameters_patch = read_ini_bool("Main", "EnableSystemParametersPatch", true, ini_path);
    settings.error_mode_patch = read_ini_bool("Main", "EnableErrorModePatch", true, ini_path);
    settings.qpc_timer_hooks = read_ini_bool("Main", "EnableQpcTimerHooks", true, ini_path);
    settings.qpc_rdtsc_hook = read_ini_bool("Main", "EnableQpcRdtscHook", true, ini_path);
    settings.timing_telemetry = read_ini_bool("Main", "EnableTimingTelemetry", false, ini_path);
    settings.crash_dumps = read_ini_bool("Main", "EnableCrashDumps", true, ini_path);
    settings.full_memory_crash_dumps = read_ini_bool("Main", "FullMemoryCrashDumps", false, ini_path);
    settings.dpi_awareness = read_ini_bool("Main", "EnableModernDpiAwareness", true, ini_path);
    settings.scheduler_precision = read_ini_bool("Main", "EnableTimeBeginPeriod", true, ini_path);
    settings.power_throttling_opt_out = read_ini_bool("Main", "EnablePowerThrottlingOptOut", true, ini_path);
    settings.miles_fallback_rate =
        read_ini_u32_in_range("Main", "MilesFallbackRate", 48000, 8000, 192000, ini_path);
    settings.miles_fallback_bits = read_ini_audio_bits(ini_path);
    settings.miles_fallback_channels = read_ini_audio_channels(ini_path);
    settings.telemetry_flush_interval_ms =
        read_ini_u32_in_range("Main", "TimingTelemetryFlushMs", 5000, 1, 60000, ini_path);
    char crash_directory_buffer[MAX_PATH] = {};
    GetPrivateProfileStringA(
        "Main",
        "CrashDumpDirectory",
        settings.crash_dump_directory.c_str(),
        crash_directory_buffer,
        static_cast<DWORD>(std::size(crash_directory_buffer)),
        ini_path.string().c_str());
    settings.crash_dump_directory = crash_directory_buffer;
    return settings;
}

Settings load_settings() {
    return load_settings_from(exe_directory() / "MHModern.ini");
}

bool enable_dpi_awareness() {
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32 != nullptr) {
        auto set_context = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (set_context != nullptr && set_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return true;
        }
    }

    HMODULE shcore = LoadLibraryA("shcore.dll");
    if (shcore != nullptr) {
        auto set_awareness = reinterpret_cast<SetProcessDpiAwarenessFn>(
            GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (set_awareness != nullptr &&
            SUCCEEDED(set_awareness(PROCESS_PER_MONITOR_DPI_AWARE))) {
            FreeLibrary(shcore);
            return true;
        }
        FreeLibrary(shcore);
    }

    return SetProcessDPIAware() == TRUE;
}

std::uint32_t execution_speed_throttling_mask(bool disable_power_throttling) {
    return disable_power_throttling ? kProcessPowerThrottlingExecutionSpeed : 0U;
}

bool is_power_throttling_unsupported_error(std::uint32_t error_code) {
    return error_code == ERROR_INVALID_FUNCTION || error_code == ERROR_NOT_SUPPORTED;
}

PowerThrottlingOptOutResult apply_power_throttling_opt_out() {
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (kernel32 == nullptr) {
        return PowerThrottlingOptOutResult::ApiUnavailable;
    }

    auto set_process_information = reinterpret_cast<SetProcessInformationFn>(
        GetProcAddress(kernel32, "SetProcessInformation"));
    if (set_process_information == nullptr) {
        return PowerThrottlingOptOutResult::ApiUnavailable;
    }

    PowerThrottlingStateCompat state{};
    state.Version = kProcessPowerThrottlingCurrentVersion;
    state.ControlMask = execution_speed_throttling_mask(true);
    state.StateMask = 0;
    if (!set_process_information(
            GetCurrentProcess(),
            kProcessPowerThrottlingInformationClass,
            &state,
            sizeof(state))) {
        const auto error = static_cast<std::uint32_t>(GetLastError());
        if (is_power_throttling_unsupported_error(error)) {
            return PowerThrottlingOptOutResult::Unsupported;
        }
        return PowerThrottlingOptOutResult::Failed;
    }

    return PowerThrottlingOptOutResult::Applied;
}

unsigned tighten_scheduler_precision() {
    HMODULE winmm = LoadLibraryA("winmm.dll");
    if (winmm == nullptr) {
        return static_cast<unsigned>(TIMERR_NOCANDO);
    }

    auto time_begin_period = reinterpret_cast<TimeBeginPeriodFn>(
        GetProcAddress(winmm, "timeBeginPeriod"));
    if (time_begin_period == nullptr) {
        FreeLibrary(winmm);
        return static_cast<unsigned>(TIMERR_NOCANDO);
    }

    const MMRESULT result = time_begin_period(1);
    FreeLibrary(winmm);
    return static_cast<unsigned>(result);
}

}  // namespace mhmodern::modern_patch::detail
