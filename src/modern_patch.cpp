#include "modern_patch.h"

#include "audio_patch.h"
#include "code_patch.h"
#include "crash_handler.h"
#include "directx_probe_patch.h"
#include "error_mode_patch.h"
#include "event_patch.h"
#include "heap_patch.h"
#include "iat_patch.h"
#include "input_patch.h"
#include "logger.h"
#include "modern_patch_internal.h"
#include "ptr_probe.h"
#include "qpc_timer.h"
#include "system_parameters_patch.h"
#include "telemetry.h"
#include "version_patch.h"
#include "wait_patch.h"

#include <windows.h>
#include <intrin.h>

namespace {
DWORD WINAPI hooked_time_get_time() {
    const auto value = mhmodern::qpc_timer::now_ms_32();
    mhmodern::telemetry::record_timer_sample(
        "timeGetTime",
        value,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    return value;
}

DWORD WINAPI hooked_get_tick_count() {
    const auto value = mhmodern::qpc_timer::now_ms_32();
    mhmodern::telemetry::record_timer_sample(
        "GetTickCount",
        value,
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    return value;
}

DWORD __cdecl hooked_time_get_time_wrapper() {
    return mhmodern::qpc_timer::now_ms_32();
}

BOOL WINAPI hooked_is_bad_read_ptr(const VOID* pointer, UINT_PTR size) {
    return mhmodern::ptr_probe::can_access_range(
               pointer,
               static_cast<std::size_t>(size),
               false,
               false)
        ? FALSE
        : TRUE;
}

BOOL WINAPI hooked_is_bad_write_ptr(LPVOID pointer, UINT_PTR size) {
    return mhmodern::ptr_probe::can_access_range(
               pointer,
               static_cast<std::size_t>(size),
               true,
               false)
        ? FALSE
        : TRUE;
}

BOOL WINAPI hooked_is_bad_code_ptr(FARPROC pointer) {
    return mhmodern::ptr_probe::can_access_range(
               reinterpret_cast<const void*>(pointer),
               1,
               false,
               true)
        ? FALSE
        : TRUE;
}

void install_timer_hooks() {
    if (!mhmodern::qpc_timer::initialize()) {
        mhmodern::logger::write("QPC timer initialization failed, timer hooks skipped");
        return;
    }

    HMODULE exe_module = GetModuleHandleA(nullptr);
    void* original_time_get_time = nullptr;
    void* original_get_tick_count = nullptr;

    const bool time_get_time_patched = mhmodern::patch_iat(
        exe_module, "WINMM.dll", "timeGetTime",
        reinterpret_cast<void*>(&hooked_time_get_time),
        &original_time_get_time);
    const bool get_tick_count_patched = mhmodern::patch_iat(
        exe_module, "KERNEL32.dll", "GetTickCount",
        reinterpret_cast<void*>(&hooked_get_tick_count),
        &original_get_tick_count);

    mhmodern::logger::write("timeGetTime patched: %s (original=%p)",
                            time_get_time_patched ? "yes" : "no",
                            original_time_get_time);
    mhmodern::logger::write("GetTickCount patched: %s (original=%p)",
                            get_tick_count_patched ? "yes" : "no",
                            original_get_tick_count);
}

void install_rdtsc_hook() {
    constexpr std::uintptr_t kRdtscHelperAddress = 0x004FB9D0;
    constexpr std::size_t kPatchSize = 10;

    auto* target = reinterpret_cast<void*>(kRdtscHelperAddress);
    const bool patched = mhmodern::patch_jmp(
        target,
        reinterpret_cast<const void*>(&mhmodern::qpc_timer::raw_counter),
        kPatchSize);
    mhmodern::logger::write("RDTSC helper patched at %p: %s",
                            target,
                            patched ? "yes" : "no");
}

void install_time_wrapper_hook() {
    constexpr std::uintptr_t kTimeWrapperAddress = 0x004C41D0;
    constexpr std::size_t kPatchSize = 6;

    auto* target = reinterpret_cast<void*>(kTimeWrapperAddress);
    const bool patched = mhmodern::patch_jmp(
        target,
        reinterpret_cast<const void*>(&hooked_time_get_time_wrapper),
        kPatchSize);
    mhmodern::logger::write("internal time wrapper patched at %p: %s",
                            target,
                            patched ? "yes" : "no");
}

void install_wait_routine_hook() {
    constexpr std::uintptr_t kWaitRoutineAddress = 0x004D76B0;
    constexpr std::size_t kPatchSize = 8;

    auto* target = reinterpret_cast<void*>(kWaitRoutineAddress);
    const bool patched = mhmodern::patch_jmp(
        target,
        reinterpret_cast<const void*>(&mhmodern::wait_patch::hooked_wait_routine),
        kPatchSize);
    mhmodern::logger::write("wait routine patched at %p: %s",
                            target,
                            patched ? "yes" : "no");
}

void install_pointer_probe_hooks() {
    HMODULE exe_module = GetModuleHandleA(nullptr);
    void* original_read = nullptr;
    void* original_code = nullptr;
    void* original_write = nullptr;

    const bool read_patched = mhmodern::patch_iat(
        exe_module,
        "KERNEL32.dll",
        "IsBadReadPtr",
        reinterpret_cast<void*>(&hooked_is_bad_read_ptr),
        &original_read);
    const bool code_patched = mhmodern::patch_iat(
        exe_module,
        "KERNEL32.dll",
        "IsBadCodePtr",
        reinterpret_cast<void*>(&hooked_is_bad_code_ptr),
        &original_code);
    const bool write_patched = mhmodern::patch_iat(
        exe_module,
        "KERNEL32.dll",
        "IsBadWritePtr",
        reinterpret_cast<void*>(&hooked_is_bad_write_ptr),
        &original_write);

    mhmodern::logger::write("IsBadReadPtr patched: %s (original=%p)",
                            read_patched ? "yes" : "no",
                            original_read);
    mhmodern::logger::write("IsBadCodePtr patched: %s (original=%p)",
                            code_patched ? "yes" : "no",
                            original_code);
    mhmodern::logger::write("IsBadWritePtr patched: %s (original=%p)",
                            write_patched ? "yes" : "no",
                            original_write);
}

}  // namespace

namespace mhmodern {

DWORD WINAPI initialize_mod(HMODULE module_handle) {
    (void)module_handle;

    logger::initialize();
    logger::write("MHModern bootstrap starting");

    const modern_patch::detail::Settings settings = modern_patch::detail::load_settings();
    logger::write("Settings: MilesAudioPatch=%d LogMilesAudioInit=%d DetectMilesPrimaryFormat=%d MilesDriverFallback=%d RawAudioBufferedOpen=%d EventPatch=%d PoolAudioTempEvents=%d LogEventPatchInit=%d HeapPatch=%d HeapTerminateOnCorruption=%d LowFragmentationHeap=%d InputPatch=%d InputAutoReacquire=%d RawMouseInput=%d LogInputInit=%d VersionPatch=%d DirectXProbePatch=%d SystemParametersPatch=%d ErrorModePatch=%d QpcTimer=%d QpcRdtsc=%d TimingTelemetry=%d CrashDumps=%d FullMemoryCrashDumps=%d DpiAwareness=%d TimeBeginPeriod=%d PowerThrottlingOptOut=%d FlushMs=%u",
                  settings.miles_audio_patch ? 1 : 0,
                  settings.log_audio_init ? 1 : 0,
                  settings.detect_miles_primary_format ? 1 : 0,
                  settings.miles_driver_fallback ? 1 : 0,
                  settings.raw_audio_buffered_open ? 1 : 0,
                  settings.event_patch ? 1 : 0,
                  settings.pool_audio_temp_events ? 1 : 0,
                  settings.log_event_init ? 1 : 0,
                  settings.heap_patch ? 1 : 0,
                  settings.heap_terminate_on_corruption ? 1 : 0,
                  settings.heap_low_fragmentation ? 1 : 0,
                  settings.input_patch ? 1 : 0,
                  settings.input_auto_reacquire ? 1 : 0,
                  settings.raw_mouse_input ? 1 : 0,
                  settings.log_input_init ? 1 : 0,
                  settings.version_patch ? 1 : 0,
                  settings.directx_probe_patch ? 1 : 0,
                  settings.system_parameters_patch ? 1 : 0,
                  settings.error_mode_patch ? 1 : 0,
                  settings.qpc_timer_hooks ? 1 : 0,
                  settings.qpc_rdtsc_hook ? 1 : 0,
                  settings.timing_telemetry ? 1 : 0,
                  settings.crash_dumps ? 1 : 0,
                  settings.full_memory_crash_dumps ? 1 : 0,
                  settings.dpi_awareness ? 1 : 0,
                  settings.scheduler_precision ? 1 : 0,
                  settings.power_throttling_opt_out ? 1 : 0,
                  settings.telemetry_flush_interval_ms);

    telemetry::configure(settings.timing_telemetry, settings.telemetry_flush_interval_ms);
    crash_handler::install({
        settings.crash_dumps,
        settings.full_memory_crash_dumps,
        settings.crash_dump_directory,
    });
    if (settings.miles_audio_patch || settings.raw_audio_buffered_open) {
        audio_patch::install({
            settings.miles_audio_patch,
            settings.log_audio_init,
            settings.detect_miles_primary_format,
            settings.miles_driver_fallback,
            settings.raw_audio_buffered_open,
            settings.miles_fallback_rate,
            settings.miles_fallback_bits,
            settings.miles_fallback_channels,
        });
    }
    if (settings.event_patch) {
        event_patch::install({
            settings.event_patch,
            settings.pool_audio_temp_events,
            settings.log_event_init,
        });
    }
    if (settings.heap_patch) {
        if (settings.miles_audio_patch) {
            logger::write(
                "Heap patch skipped: incompatible with Miles startup on Manhunt 1");
        } else {
            heap_patch::install({
                settings.heap_patch,
                settings.heap_terminate_on_corruption,
                settings.heap_low_fragmentation,
            });
        }
    }
    if (settings.input_patch) {
        input_patch::install({
            settings.input_patch,
            settings.input_auto_reacquire,
            settings.raw_mouse_input,
            settings.log_input_init,
        });
    }
    if (settings.version_patch) {
        version_patch::install({
            settings.version_patch,
        });
    }
    if (settings.directx_probe_patch) {
        directx_probe_patch::install({
            settings.directx_probe_patch,
        });
    }
    if (settings.system_parameters_patch) {
        system_parameters_patch::install({
            settings.system_parameters_patch,
        });
    }
    if (settings.error_mode_patch) {
        error_mode_patch::install({
            settings.error_mode_patch,
        });
    }

    if (settings.power_throttling_opt_out) {
        switch (modern_patch::detail::apply_power_throttling_opt_out()) {
        case modern_patch::detail::PowerThrottlingOptOutResult::Applied:
            logger::write("Power throttling opt-out applied");
            break;
        case modern_patch::detail::PowerThrottlingOptOutResult::ApiUnavailable:
            logger::write("Power throttling opt-out skipped: SetProcessInformation unavailable");
            break;
        case modern_patch::detail::PowerThrottlingOptOutResult::Unsupported:
            logger::write("Power throttling opt-out skipped: unsupported on this platform");
            break;
        case modern_patch::detail::PowerThrottlingOptOutResult::Failed:
            logger::write(
                "Power throttling opt-out failed: gle=%lu",
                static_cast<unsigned long>(GetLastError()));
            break;
        }
    }

    if (settings.dpi_awareness) {
        logger::write(
            "DPI awareness status: %s",
            modern_patch::detail::enable_dpi_awareness() ? "enabled" : "failed");
    }

    if (settings.scheduler_precision) {
        logger::write(
            "timeBeginPeriod(1) returned %u",
            modern_patch::detail::tighten_scheduler_precision());
    }

    if (settings.qpc_timer_hooks) {
        install_timer_hooks();
    }

    if (settings.qpc_rdtsc_hook) {
        install_rdtsc_hook();
    }

    install_time_wrapper_hook();

    install_wait_routine_hook();

    install_pointer_probe_hooks();

    logger::write("MHModern bootstrap complete");
    return 0;
}

}  // namespace mhmodern
