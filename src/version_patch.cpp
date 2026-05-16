#include "version_patch.h"

#include "iat_patch.h"
#include "logger.h"

#include <winternl.h>

namespace {

using GetVersionFn = DWORD(WINAPI*)();
using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

GetVersionFn g_original_get_version = nullptr;

DWORD fallback_get_version() {
    return g_original_get_version != nullptr ? g_original_get_version() : 0;
}

DWORD WINAPI hooked_get_version() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == nullptr) {
        return fallback_get_version();
    }

    auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (rtl_get_version == nullptr) {
        return fallback_get_version();
    }

    RTL_OSVERSIONINFOW version_info{};
    version_info.dwOSVersionInfoSize = sizeof(version_info);
    if (rtl_get_version(&version_info) < 0) {
        return fallback_get_version();
    }

    return mhmodern::version_patch::detail::pack_get_version_result(
        version_info.dwMajorVersion,
        version_info.dwMinorVersion,
        version_info.dwBuildNumber);
}

}  // namespace

namespace mhmodern::version_patch::detail {

DWORD pack_get_version_result(DWORD major, DWORD minor, DWORD build) {
    return ((build & 0x7FFFu) << 16) | ((minor & 0xFFu) << 8) | (major & 0xFFu);
}

}  // namespace mhmodern::version_patch::detail

namespace mhmodern::version_patch {

bool install(const Settings& settings) {
    if (!settings.enabled) {
        return true;
    }

    HMODULE exe_module = GetModuleHandleA(nullptr);
    void* original = nullptr;
    const bool patched = mhmodern::patch_iat(
        exe_module,
        "KERNEL32.dll",
        "GetVersion",
        reinterpret_cast<void*>(&hooked_get_version),
        &original);
    g_original_get_version = reinterpret_cast<GetVersionFn>(original);

    mhmodern::logger::write(
        "Version patch: GetVersion=%s original=%p",
        patched ? "yes" : "no",
        original);
    return patched;
}

}  // namespace mhmodern::version_patch
