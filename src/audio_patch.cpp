#include "audio_patch.h"
#include "iat_patch.h"
#include "logger.h"

#include <windows.h>

#include <array>

namespace {

using AilStartupFn = int(__stdcall*)();
using AilOpenDigitalDriverFn = void* (__stdcall*)(std::uint32_t, std::int32_t, std::int32_t, std::uint32_t);
using AilLastErrorFn = const char* (__stdcall*)();
using CreateFileAFn = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

mhmodern::audio_patch::Settings g_settings{};
AilStartupFn g_original_startup = nullptr;
AilOpenDigitalDriverFn g_original_open_digital_driver = nullptr;
AilLastErrorFn g_last_error = nullptr;
CreateFileAFn g_original_create_file_a = nullptr;

constexpr DWORD kFileFlagNoBuffering = 0x20000000u;

const char* miles_last_error() {
    if (g_last_error == nullptr) {
        return "unknown";
    }
    const char* message = g_last_error();
    return message != nullptr && message[0] != '\0' ? message : "unknown";
}

void log_driver_attempt(
    const char* label,
    std::uint32_t rate,
    std::int32_t bits,
    std::int32_t channels,
    std::uint32_t flags,
    void* handle) {
    if (!g_settings.log_audio_init) {
        return;
    }

    mhmodern::logger::write(
        "Miles driver %s: rate=%u bits=%d channels=%d flags=%u handle=%p error=%s",
        label,
        rate,
        bits,
        channels,
        flags,
        handle,
        handle != nullptr ? "none" : miles_last_error());
}

int __stdcall hooked_ail_startup() {
    const int result = g_original_startup != nullptr ? g_original_startup() : 0;
    if (g_settings.log_audio_init) {
        mhmodern::logger::write(
            "AIL_startup result=%d error=%s",
            result,
            result != 0 ? "none" : miles_last_error());
    }
    return result;
}

void* __stdcall hooked_ail_open_digital_driver(
    std::uint32_t sample_rate,
    std::int32_t bits,
    std::int32_t channels,
    std::uint32_t flags) {
    if (g_original_open_digital_driver == nullptr) {
        return nullptr;
    }

    void* handle = g_original_open_digital_driver(sample_rate, bits, channels, flags);
    log_driver_attempt("request", sample_rate, bits, channels, flags, handle);
    if (handle != nullptr || !g_settings.miles_driver_fallback) {
        return handle;
    }

    const auto try_attempt = [flags](const char* label, const mhmodern::audio_patch::DriverOpenAttempt& attempt) {
        void* attempt_handle = g_original_open_digital_driver(
            attempt.sample_rate,
            attempt.bits,
            attempt.channels,
            flags);
        log_driver_attempt(label, attempt.sample_rate, attempt.bits, attempt.channels, flags, attempt_handle);
        return attempt_handle;
    };

    if (g_settings.detect_default_device_format) {
        const auto detected = mhmodern::audio_patch::detail::query_default_device_format();
        if (detected.has_value()) {
            if (g_settings.log_audio_init) {
                mhmodern::logger::write(
                    "Detected default audio format for Miles: rate=%u bits=%d channels=%d",
                    detected->sample_rate,
                    detected->bits,
                    detected->channels);
            }

            if (!(detected->sample_rate == sample_rate &&
                  detected->bits == bits &&
                  detected->channels == channels)) {
                handle = try_attempt("detected", *detected);
                if (handle != nullptr) {
                    return handle;
                }
            }
        } else if (g_settings.log_audio_init) {
            mhmodern::logger::write(
                "Detected default audio format for Miles: unavailable, using configured fallback");
        }
    }

    std::array<mhmodern::audio_patch::DriverOpenAttempt, 3> attempts{};
    const std::size_t attempt_count = mhmodern::audio_patch::detail::build_driver_attempts(
        sample_rate,
        bits,
        channels,
        g_settings.fallback_rate,
        g_settings.fallback_bits,
        g_settings.fallback_channels,
        attempts);
    for (std::size_t index = 0; index < attempt_count; ++index) {
        const auto& attempt = attempts[index];
        if (attempt.sample_rate == sample_rate &&
            attempt.bits == bits &&
            attempt.channels == channels) {
            continue;
        }
        handle = try_attempt("fallback", attempt);
        if (handle != nullptr) {
            return handle;
        }
    }

    return nullptr;
}

HANDLE WINAPI hooked_create_file_a(
    LPCSTR file_name,
    DWORD desired_access,
    DWORD share_mode,
    LPSECURITY_ATTRIBUTES security_attributes,
    DWORD creation_disposition,
    DWORD flags_and_attributes,
    HANDLE template_file) {
    if (g_original_create_file_a == nullptr) {
        return INVALID_HANDLE_VALUE;
    }

    DWORD adjusted_flags = flags_and_attributes;
    if (g_settings.raw_audio_buffered_open &&
        mhmodern::audio_patch::detail::should_buffer_raw_audio_open(
            file_name,
            desired_access,
            share_mode,
            creation_disposition,
            flags_and_attributes)) {
        adjusted_flags &= ~kFileFlagNoBuffering;
        if (g_settings.log_audio_init) {
            mhmodern::logger::write(
                "Raw SFX buffered open: file=%s flags=%08X -> %08X",
                file_name != nullptr ? file_name : "(null)",
                flags_and_attributes,
                adjusted_flags);
        }
    }

    return g_original_create_file_a(
        file_name,
        desired_access,
        share_mode,
        security_attributes,
        creation_disposition,
        adjusted_flags,
        template_file);
}

}  // namespace

namespace mhmodern::audio_patch {

bool install(const Settings& settings) {
    g_settings = settings;

    HMODULE exe_module = GetModuleHandleA(nullptr);
    HMODULE mss_module = GetModuleHandleA("mss32.dll");
    if (exe_module == nullptr) {
        mhmodern::logger::write("Miles audio patch skipped: exe module not available");
        return false;
    }

    bool patched_any = false;

    if (settings.miles_audio_patch) {
        if (mss_module == nullptr) {
            mhmodern::logger::write("Miles audio patch skipped: mss32.dll not loaded");
            g_last_error = nullptr;
        } else {
            g_last_error = reinterpret_cast<AilLastErrorFn>(GetProcAddress(mss_module, "_AIL_last_error@0"));

            void* startup_original = nullptr;
            void* open_driver_original = nullptr;
            const bool startup_patched = mhmodern::patch_iat(
                exe_module,
                "mss32.dll",
                "_AIL_startup@0",
                reinterpret_cast<void*>(&hooked_ail_startup),
                &startup_original);
            const bool open_driver_patched = mhmodern::patch_iat(
                exe_module,
                "mss32.dll",
                "_AIL_open_digital_driver@16",
                reinterpret_cast<void*>(&hooked_ail_open_digital_driver),
                &open_driver_original);

            g_original_startup = reinterpret_cast<AilStartupFn>(startup_original);
            g_original_open_digital_driver = reinterpret_cast<AilOpenDigitalDriverFn>(open_driver_original);

            mhmodern::logger::write(
                "Miles audio patch: startup=%s openDigitalDriver=%s lastError=%p",
                startup_patched ? "yes" : "no",
                open_driver_patched ? "yes" : "no",
                reinterpret_cast<const void*>(g_last_error));
            patched_any = startup_patched || open_driver_patched;
        }
    }

    if (settings.raw_audio_buffered_open) {
        void* create_file_original = nullptr;
        const bool create_file_patched = mhmodern::patch_iat(
            exe_module,
            "KERNEL32.dll",
            "CreateFileA",
            reinterpret_cast<void*>(&hooked_create_file_a),
            &create_file_original);
        g_original_create_file_a = reinterpret_cast<CreateFileAFn>(create_file_original);
        if (g_settings.log_audio_init) {
            mhmodern::logger::write(
                "Raw SFX buffered-open patch: %s original=%p",
                create_file_patched ? "yes" : "no",
                create_file_original);
        }
        patched_any = patched_any || create_file_patched;
    }

    return patched_any;
}

}  // namespace mhmodern::audio_patch
