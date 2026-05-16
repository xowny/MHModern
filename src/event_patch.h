#pragma once

#include <windows.h>

#include <cstdint>

namespace mhmodern::event_patch {

struct Settings {
    bool enabled = true;
    bool pool_audio_streamer_temp_events = true;
    bool log_init = false;
};

bool install(const Settings& settings);

namespace detail {
bool should_pool_audio_temp_event(
    std::uintptr_t caller,
    LPSECURITY_ATTRIBUTES security_attributes,
    BOOL manual_reset,
    BOOL initial_state,
    LPCSTR name);
}

}  // namespace mhmodern::event_patch
