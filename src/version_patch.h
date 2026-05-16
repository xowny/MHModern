#pragma once

#include <windows.h>

namespace mhmodern::version_patch {

struct Settings {
    bool enabled = true;
};

bool install(const Settings& settings);

namespace detail {
DWORD pack_get_version_result(DWORD major, DWORD minor, DWORD build);
}

}  // namespace mhmodern::version_patch
