#include "system_parameters_patch.h"

#include "iat_patch.h"
#include "logger.h"

#include <intrin.h>

namespace {

using SystemParametersInfoAFn = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT);

constexpr std::uintptr_t kMainBootstrapStart = 0x004C1080;
constexpr std::uintptr_t kMainBootstrapEnd = 0x004C1800;

SystemParametersInfoAFn g_original_system_parameters_info_a = nullptr;

BOOL WINAPI hooked_system_parameters_info_a(
    UINT action,
    UINT ui_param,
    PVOID pv_param,
    UINT win_ini) {
    (void)ui_param;
    (void)pv_param;
    (void)win_ini;

    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (mhmodern::system_parameters_patch::detail::should_suppress_spi_call(caller, action)) {
        return TRUE;
    }

    if (g_original_system_parameters_info_a == nullptr) {
        return FALSE;
    }
    return g_original_system_parameters_info_a(action, ui_param, pv_param, win_ini);
}

}  // namespace

namespace mhmodern::system_parameters_patch::detail {

bool should_suppress_spi_call(const std::uintptr_t caller, const UINT action) {
    if (caller < kMainBootstrapStart || caller >= kMainBootstrapEnd) {
        return false;
    }

    switch (action) {
    case SPI_SETSCREENSAVEACTIVE:
    case SPI_SETLOWPOWERACTIVE:
    case SPI_SETPOWEROFFACTIVE:
    case SPI_SETSTICKYKEYS:
    case SPI_SETFOREGROUNDLOCKTIMEOUT:
        return true;
    default:
        return false;
    }
}

}  // namespace mhmodern::system_parameters_patch::detail

namespace mhmodern::system_parameters_patch {

bool install(const Settings& settings) {
    if (!settings.enabled) {
        return true;
    }

    HMODULE exe_module = GetModuleHandleA(nullptr);
    void* original = nullptr;
    const bool patched = mhmodern::patch_iat(
        exe_module,
        "USER32.dll",
        "SystemParametersInfoA",
        reinterpret_cast<void*>(&hooked_system_parameters_info_a),
        &original);
    g_original_system_parameters_info_a = reinterpret_cast<SystemParametersInfoAFn>(original);

    mhmodern::logger::write(
        "SystemParameters patch: SystemParametersInfoA=%s suppressStartupGlobalWrites=1",
        patched ? "yes" : "no");
    return patched;
}

}  // namespace mhmodern::system_parameters_patch
