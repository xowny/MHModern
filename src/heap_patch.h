#pragma once

#include <windows.h>

namespace mhmodern::heap_patch {

struct Settings {
    bool enabled = true;
    bool terminate_on_corruption = true;
    bool low_fragmentation_heap = true;
};

bool install(const Settings& settings);

namespace detail {
bool should_enable_lfh(HANDLE heap, HANDLE process_heap, bool low_fragmentation_heap_enabled);
}

}  // namespace mhmodern::heap_patch
