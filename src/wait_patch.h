#pragma once

#include <cstdint>

namespace mhmodern::wait_patch {

std::uint32_t wrapped_elapsed(std::uint32_t base, std::uint32_t now);
std::uint32_t remaining_delay(std::uint32_t previous_delta, std::uint32_t current_delta, std::uint32_t minimum_step);
void __cdecl hooked_wait_routine();

}  // namespace mhmodern::wait_patch
