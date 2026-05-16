#pragma once

#include <windows.h>

#include <cstdint>

namespace mhmodern::error_mode_patch {

struct Settings {
    bool enabled = true;
};

bool install(const Settings& settings);

namespace detail {
bool is_bootstrap_caller(std::uintptr_t caller);
UINT translate_error_mode_request(
    std::uintptr_t caller,
    UINT requested_mode,
    bool has_saved_mode,
    UINT saved_mode);
}

}  // namespace mhmodern::error_mode_patch
