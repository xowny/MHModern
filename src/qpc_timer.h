#pragma once

#include <windows.h>

#include <cstdint>

namespace mhmodern::time_math {

uint64_t counter_to_milliseconds(int64_t counter_delta, int64_t frequency);
uint32_t wrap_milliseconds(uint64_t milliseconds);
uint32_t lower_32_bits(uint64_t value);
uint32_t upper_32_bits(uint64_t value);

}  // namespace mhmodern::time_math

namespace mhmodern::qpc_timer {

bool initialize();
uint64_t frequency();
uint64_t raw_counter();
uint64_t now_ms_64();
uint32_t now_ms_32();

}  // namespace mhmodern::qpc_timer
