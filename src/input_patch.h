#pragma once

#include <cstdint>
#include <windows.h>

namespace mhmodern::input_patch {

struct Settings {
    bool enabled = true;
    bool auto_reacquire = true;
    bool raw_mouse_input = true;
    bool log_input_init = false;
};

bool install(const Settings& settings);

namespace detail {

struct MouseDelta {
    LONG x = 0;
    LONG y = 0;
};

bool should_retry_input_hresult(HRESULT result);
bool should_retry_get_device_state_request(HRESULT result, DWORD data_size, const void* data);
bool should_wrap_mouse_guid(REFGUID guid);
LONG saturating_long_from_delta(std::int64_t value);
MouseDelta choose_mouse_delta(bool enable_raw_mouse, std::int64_t raw_x, std::int64_t raw_y, LONG fallback_x, LONG fallback_y);

}  // namespace detail

}  // namespace mhmodern::input_patch
