#include "directx_probe_patch.h"

#include "code_patch.h"
#include "logger.h"

#include <cstdint>

namespace {

constexpr std::uintptr_t kDirectXProbeAddress = 0x004BECC0;
constexpr std::size_t kDirectXProbePatchSize = 5;

std::uint32_t __cdecl hooked_directx_probe() {
    HMODULE ddraw = LoadLibraryA("DDRAW.DLL");
    if (ddraw == nullptr) {
        OutputDebugStringA("MHModern: DDRAW.DLL missing during DirectX probe");
        return 0;
    }

    const auto direct_draw_create_ex = GetProcAddress(ddraw, "DirectDrawCreateEx");
    if (direct_draw_create_ex == nullptr) {
        FreeLibrary(ddraw);
        OutputDebugStringA("MHModern: DirectDrawCreateEx missing during DirectX probe");
        return 0;
    }

    HMODULE d3d8 = LoadLibraryA("D3D8.DLL");
    if (d3d8 == nullptr) {
        FreeLibrary(ddraw);
        return 0x700;
    }

    HMODULE dpnhpast = LoadLibraryA("dpnhpast.dll");
    if (dpnhpast == nullptr) {
        OutputDebugStringA("MHModern: dpnhpast.dll missing, treating it as optional");
    }

    HMODULE d3d9 = LoadLibraryA("D3D9.DLL");
    if (d3d9 == nullptr) {
        if (dpnhpast != nullptr) {
            FreeLibrary(dpnhpast);
        }
        FreeLibrary(d3d8);
        FreeLibrary(ddraw);
        return 0x801;
    }

    if (dpnhpast != nullptr) {
        FreeLibrary(dpnhpast);
    }
    FreeLibrary(d3d9);
    FreeLibrary(d3d8);
    FreeLibrary(ddraw);
    return 0x900;
}

}  // namespace

namespace mhmodern::directx_probe_patch::detail {

std::uint32_t compute_probe_result(
    const bool has_ddraw,
    const bool has_direct_draw_create_ex,
    const bool has_d3d8,
    const bool has_dpnhpast,
    const bool has_d3d9) {
    (void)has_dpnhpast;

    if (!has_ddraw || !has_direct_draw_create_ex) {
        return 0;
    }
    if (!has_d3d8) {
        return 0x700;
    }
    if (!has_d3d9) {
        return 0x801;
    }
    return 0x900;
}

}  // namespace mhmodern::directx_probe_patch::detail

namespace mhmodern::directx_probe_patch {

bool install(const Settings& settings) {
    if (!settings.enabled) {
        return true;
    }

    auto* target = reinterpret_cast<void*>(kDirectXProbeAddress);
    const bool patched = mhmodern::patch_jmp(
        target,
        reinterpret_cast<const void*>(&hooked_directx_probe),
        kDirectXProbePatchSize);
    mhmodern::logger::write(
        "DirectX probe patch: 004BECC0=%s optionalDpnhpast=1",
        patched ? "yes" : "no");
    return patched;
}

}  // namespace mhmodern::directx_probe_patch
