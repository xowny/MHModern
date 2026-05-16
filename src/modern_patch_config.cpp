#include "modern_patch_internal.h"

#include <windows.h>
#include <mmsystem.h>
#include <shellscalingapi.h>

#include <filesystem>

namespace {

using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(PROCESS_DPI_AWARENESS);
using TimeBeginPeriodFn = MMRESULT(WINAPI*)(UINT);

std::filesystem::path exe_directory() {
    char buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) {
        return std::filesystem::current_path();
    }

    return std::filesystem::path(buffer).parent_path();
}

}  // namespace

namespace mhmodern::modern_patch::detail {

Settings load_settings_from(const std::filesystem::path& ini_path) {
    Settings settings{};
    settings.miles_audio_patch =
        GetPrivateProfileIntA("Main", "EnableMilesAudioPatch", 1, ini_path.string().c_str()) != 0;
    settings.log_audio_init =
        GetPrivateProfileIntA("Main", "LogMilesAudioInit", 0, ini_path.string().c_str()) != 0;
    settings.detect_miles_primary_format =
        GetPrivateProfileIntA("Main", "DetectMilesPrimaryFormat", 1, ini_path.string().c_str()) != 0;
    settings.miles_driver_fallback =
        GetPrivateProfileIntA("Main", "EnableMilesDriverFallback", 1, ini_path.string().c_str()) != 0;
    settings.raw_audio_buffered_open =
        GetPrivateProfileIntA("Main", "EnableRawAudioBufferedOpen", 1, ini_path.string().c_str()) != 0;
    settings.event_patch =
        GetPrivateProfileIntA("Main", "EnableEventPatch", 0, ini_path.string().c_str()) != 0;
    settings.pool_audio_temp_events =
        GetPrivateProfileIntA("Main", "PoolAudioTempEvents", 0, ini_path.string().c_str()) != 0;
    settings.log_event_init =
        GetPrivateProfileIntA("Main", "LogEventPatchInit", 0, ini_path.string().c_str()) != 0;
    settings.heap_patch =
        GetPrivateProfileIntA("Main", "EnableHeapPatch", 0, ini_path.string().c_str()) != 0;
    settings.heap_terminate_on_corruption =
        GetPrivateProfileIntA("Main", "EnableHeapTerminateOnCorruption", 1, ini_path.string().c_str()) != 0;
    settings.heap_low_fragmentation =
        GetPrivateProfileIntA("Main", "EnableLowFragmentationHeap", 1, ini_path.string().c_str()) != 0;
    settings.input_patch =
        GetPrivateProfileIntA("Main", "EnableInputPatch", 1, ini_path.string().c_str()) != 0;
    settings.input_auto_reacquire =
        GetPrivateProfileIntA("Main", "EnableInputAutoReacquire", 1, ini_path.string().c_str()) != 0;
    settings.log_input_init =
        GetPrivateProfileIntA("Main", "LogInputInit", 0, ini_path.string().c_str()) != 0;
    settings.version_patch =
        GetPrivateProfileIntA("Main", "EnableVersionPatch", 1, ini_path.string().c_str()) != 0;
    settings.directx_probe_patch =
        GetPrivateProfileIntA("Main", "EnableDirectXProbePatch", 1, ini_path.string().c_str()) != 0;
    settings.system_parameters_patch =
        GetPrivateProfileIntA("Main", "EnableSystemParametersPatch", 1, ini_path.string().c_str()) != 0;
    settings.error_mode_patch =
        GetPrivateProfileIntA("Main", "EnableErrorModePatch", 1, ini_path.string().c_str()) != 0;
    settings.qpc_timer_hooks =
        GetPrivateProfileIntA("Main", "EnableQpcTimerHooks", 1, ini_path.string().c_str()) != 0;
    settings.qpc_rdtsc_hook =
        GetPrivateProfileIntA("Main", "EnableQpcRdtscHook", 1, ini_path.string().c_str()) != 0;
    settings.timing_telemetry =
        GetPrivateProfileIntA("Main", "EnableTimingTelemetry", 0, ini_path.string().c_str()) != 0;
    settings.crash_dumps =
        GetPrivateProfileIntA("Main", "EnableCrashDumps", 1, ini_path.string().c_str()) != 0;
    settings.full_memory_crash_dumps =
        GetPrivateProfileIntA("Main", "FullMemoryCrashDumps", 0, ini_path.string().c_str()) != 0;
    settings.dpi_awareness =
        GetPrivateProfileIntA("Main", "EnableModernDpiAwareness", 1, ini_path.string().c_str()) != 0;
    settings.scheduler_precision =
        GetPrivateProfileIntA("Main", "EnableTimeBeginPeriod", 1, ini_path.string().c_str()) != 0;
    settings.miles_fallback_rate = static_cast<std::uint32_t>(
        GetPrivateProfileIntA("Main", "MilesFallbackRate", 48000, ini_path.string().c_str()));
    settings.miles_fallback_bits =
        GetPrivateProfileIntA("Main", "MilesFallbackBits", 16, ini_path.string().c_str());
    settings.miles_fallback_channels =
        GetPrivateProfileIntA("Main", "MilesFallbackChannels", 2, ini_path.string().c_str());
    settings.telemetry_flush_interval_ms = static_cast<std::uint32_t>(
        GetPrivateProfileIntA("Main", "TimingTelemetryFlushMs", 5000, ini_path.string().c_str()));
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
