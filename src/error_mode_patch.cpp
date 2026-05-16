#include "error_mode_patch.h"

#include "iat_patch.h"
#include "logger.h"

#include <intrin.h>

namespace {

using SetErrorModeFn = UINT(WINAPI*)(UINT);

constexpr std::uintptr_t kMainBootstrapStart = 0x004C1080;
constexpr std::uintptr_t kMainBootstrapEnd = 0x004C1800;

SetErrorModeFn g_original_set_error_mode = nullptr;
bool g_has_saved_mode = false;
UINT g_saved_mode = 0;

UINT WINAPI hooked_set_error_mode(UINT requested_mode) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const UINT translated_mode = mhmodern::error_mode_patch::detail::translate_error_mode_request(
        caller,
        requested_mode,
        g_has_saved_mode,
        g_saved_mode);

    if (g_original_set_error_mode == nullptr) {
        return 0;
    }

    const UINT previous_mode = g_original_set_error_mode(translated_mode);
    if (mhmodern::error_mode_patch::detail::is_bootstrap_caller(caller) && requested_mode == SEM_FAILCRITICALERRORS) {
        g_saved_mode = previous_mode;
        g_has_saved_mode = true;
    }
    if (mhmodern::error_mode_patch::detail::is_bootstrap_caller(caller) && requested_mode == 0 && g_has_saved_mode) {
        g_has_saved_mode = false;
    }
    return previous_mode;
}

}  // namespace

namespace mhmodern::error_mode_patch::detail {

bool is_bootstrap_caller(const std::uintptr_t caller) {
    return caller >= kMainBootstrapStart && caller < kMainBootstrapEnd;
}

UINT translate_error_mode_request(
    const std::uintptr_t caller,
    const UINT requested_mode,
    const bool has_saved_mode,
    const UINT saved_mode) {
    if (!is_bootstrap_caller(caller)) {
        return requested_mode;
    }

    if (requested_mode == 0 && has_saved_mode) {
        return saved_mode;
    }

    return requested_mode;
}

}  // namespace mhmodern::error_mode_patch::detail

namespace mhmodern::error_mode_patch {

bool install(const Settings& settings) {
    if (!settings.enabled) {
        return true;
    }

    HMODULE exe_module = GetModuleHandleA(nullptr);
    void* original = nullptr;
    const bool patched = mhmodern::patch_iat(
        exe_module,
        "KERNEL32.dll",
        "SetErrorMode",
        reinterpret_cast<void*>(&hooked_set_error_mode),
        &original);
    g_original_set_error_mode = reinterpret_cast<SetErrorModeFn>(original);

    mhmodern::logger::write(
        "Error mode patch: SetErrorMode=%s restorePriorMode=1",
        patched ? "yes" : "no");
    return patched;
}

}  // namespace mhmodern::error_mode_patch
