#pragma once

#include <windows.h>

#include <cstdint>

namespace mhmodern::system_parameters_patch {

struct Settings {
    bool enabled = true;
};

bool install(const Settings& settings);

namespace detail {
bool should_suppress_spi_call(std::uintptr_t caller, UINT action);
}

}  // namespace mhmodern::system_parameters_patch
