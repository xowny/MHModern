#pragma once

#include <windows.h>

#include <cstdint>

namespace mhmodern::directx_probe_patch {

struct Settings {
    bool enabled = true;
};

bool install(const Settings& settings);

namespace detail {
std::uint32_t compute_probe_result(
    bool has_ddraw,
    bool has_direct_draw_create_ex,
    bool has_d3d8,
    bool has_dpnhpast,
    bool has_d3d9);
}

}  // namespace mhmodern::directx_probe_patch
