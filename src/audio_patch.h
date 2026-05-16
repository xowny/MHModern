#pragma once

#include <optional>
#include <array>
#include <cstddef>
#include <cstdint>

namespace mhmodern::audio_patch {

struct DriverOpenAttempt {
    std::uint32_t sample_rate = 0;
    std::int32_t bits = 0;
    std::int32_t channels = 0;
};

struct Settings {
    bool miles_audio_patch = true;
    bool log_audio_init = true;
    bool detect_default_device_format = true;
    bool miles_driver_fallback = true;
    bool raw_audio_buffered_open = true;
    std::uint32_t fallback_rate = 48000;
    std::int32_t fallback_bits = 16;
    std::int32_t fallback_channels = 2;
};

bool install(const Settings& settings);

namespace detail {
std::optional<DriverOpenAttempt> query_default_device_format();
DriverOpenAttempt normalize_miles_driver_format(
    std::uint32_t sample_rate,
    std::int32_t bits,
    std::int32_t channels);
std::size_t build_driver_attempts(
    std::uint32_t requested_rate,
    std::int32_t requested_bits,
    std::int32_t requested_channels,
    std::uint32_t fallback_rate,
    std::int32_t fallback_bits,
    std::int32_t fallback_channels,
    std::array<DriverOpenAttempt, 3>& attempts);
bool should_buffer_raw_audio_open(
    const char* file_name,
    std::uint32_t desired_access,
    std::uint32_t share_mode,
    std::uint32_t creation_disposition,
    std::uint32_t flags_and_attributes);
}

}  // namespace mhmodern::audio_patch
