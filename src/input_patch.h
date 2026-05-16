#pragma once

#include <windows.h>

namespace mhmodern::input_patch {

struct Settings {
    bool enabled = true;
    bool auto_reacquire = true;
    bool log_input_init = false;
};

bool install(const Settings& settings);

namespace detail {
bool should_retry_input_hresult(HRESULT result);
}

}  // namespace mhmodern::input_patch
