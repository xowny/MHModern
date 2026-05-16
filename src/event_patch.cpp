#include "event_patch.h"

#include "iat_patch.h"
#include "logger.h"

#include <windows.h>
#include <intrin.h>

namespace {

using CreateEventAFn = HANDLE(WINAPI*)(
    LPSECURITY_ATTRIBUTES,
    BOOL,
    BOOL,
    LPCSTR);
using CloseHandleFn = BOOL(WINAPI*)(HANDLE);

constexpr std::uintptr_t kAudioInitReadCallerA = 0x00458575;
constexpr std::uintptr_t kAudioInitReadCallerB = 0x00459F0A;

mhmodern::event_patch::Settings g_settings{};
CreateEventAFn g_original_create_event_a = nullptr;
CloseHandleFn g_original_close_handle = nullptr;
HANDLE g_pooled_audio_temp_event = nullptr;
LONG g_pooled_event_use_count = 0;

HANDLE ensure_pooled_audio_temp_event() {
    if (g_original_create_event_a == nullptr) {
        return nullptr;
    }

    HANDLE existing = reinterpret_cast<HANDLE>(
        InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID*>(&g_pooled_audio_temp_event),
            nullptr,
            nullptr));
    if (existing != nullptr) {
        return existing;
    }

    HANDLE created = g_original_create_event_a(nullptr, FALSE, FALSE, nullptr);
    if (created == nullptr) {
        return nullptr;
    }

    PVOID prior = InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID*>(&g_pooled_audio_temp_event),
        created,
        nullptr);
    if (prior != nullptr) {
        g_original_close_handle(created);
        return reinterpret_cast<HANDLE>(prior);
    }

    if (g_settings.log_init) {
        mhmodern::logger::write(
            "Event patch: created pooled audio temp event handle=%p",
            created);
    }
    return created;
}

HANDLE WINAPI hooked_create_event_a(
    LPSECURITY_ATTRIBUTES security_attributes,
    BOOL manual_reset,
    BOOL initial_state,
    LPCSTR name) {
    if (g_original_create_event_a == nullptr) {
        return nullptr;
    }

    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (g_settings.pool_audio_streamer_temp_events &&
        mhmodern::event_patch::detail::should_pool_audio_temp_event(
            caller,
            security_attributes,
            manual_reset,
            initial_state,
            name)) {
        HANDLE pooled = ensure_pooled_audio_temp_event();
        if (pooled != nullptr) {
            ResetEvent(pooled);
            InterlockedIncrement(&g_pooled_event_use_count);
            return pooled;
        }
    }

    return g_original_create_event_a(
        security_attributes,
        manual_reset,
        initial_state,
        name);
}

BOOL WINAPI hooked_close_handle(HANDLE object) {
    if (object != nullptr && object == g_pooled_audio_temp_event) {
        return TRUE;
    }

    if (g_original_close_handle == nullptr) {
        return FALSE;
    }

    return g_original_close_handle(object);
}

}  // namespace

namespace mhmodern::event_patch::detail {

bool should_pool_audio_temp_event(
    std::uintptr_t caller,
    LPSECURITY_ATTRIBUTES security_attributes,
    BOOL manual_reset,
    BOOL initial_state,
    LPCSTR name) {
    if (caller != kAudioInitReadCallerA && caller != kAudioInitReadCallerB) {
        return false;
    }

    if (security_attributes != nullptr || manual_reset != FALSE || initial_state != FALSE) {
        return false;
    }

    return name == nullptr;
}

}  // namespace mhmodern::event_patch::detail

namespace mhmodern::event_patch {

bool install(const Settings& settings) {
    g_settings = settings;
    if (!settings.enabled) {
        return true;
    }

    HMODULE exe_module = GetModuleHandleA(nullptr);
    void* original_create_event = nullptr;
    void* original_close = nullptr;

    const bool create_patched = mhmodern::patch_iat(
        exe_module,
        "KERNEL32.dll",
        "CreateEventA",
        reinterpret_cast<void*>(&hooked_create_event_a),
        &original_create_event);
    const bool close_patched = mhmodern::patch_iat(
        exe_module,
        "KERNEL32.dll",
        "CloseHandle",
        reinterpret_cast<void*>(&hooked_close_handle),
        &original_close);

    g_original_create_event_a = reinterpret_cast<CreateEventAFn>(original_create_event);
    g_original_close_handle = reinterpret_cast<CloseHandleFn>(original_close);

    mhmodern::logger::write(
        "Event patch: CreateEventA=%s CloseHandle=%s poolAudioTempEvents=%d createOriginal=%p closeOriginal=%p",
        create_patched ? "yes" : "no",
        close_patched ? "yes" : "no",
        settings.pool_audio_streamer_temp_events ? 1 : 0,
        original_create_event,
        original_close);
    return create_patched && close_patched;
}

}  // namespace mhmodern::event_patch
