#include "../src/audio_patch.h"

#include <iostream>

namespace {

bool expect_true(const char* label, bool value) {
    if (!value) {
        std::cerr << label << " failed\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    auto normalized = mhmodern::audio_patch::detail::normalize_miles_driver_format(48000, 24, 2);
    ok &= expect_true("normalize keeps sample rate", normalized.sample_rate == 48000);
    ok &= expect_true("normalize keeps 24-bit depth", normalized.bits == 24);
    ok &= expect_true("normalize keeps stereo", normalized.channels == 2);

    normalized = mhmodern::audio_patch::detail::normalize_miles_driver_format(0, 32, 6);
    ok &= expect_true("normalize fills default sample rate", normalized.sample_rate == 48000);
    ok &= expect_true("normalize keeps 32-bit depth", normalized.bits == 32);
    ok &= expect_true("normalize clamps multichannel to stereo", normalized.channels == 2);

    normalized = mhmodern::audio_patch::detail::normalize_miles_driver_format(0, 20, 6);
    ok &= expect_true("normalize fills safe bits for unsupported depth", normalized.bits == 16);

    ok &= expect_true(
        "raw sfx open matches exact startup path",
        mhmodern::audio_patch::detail::should_buffer_raw_audio_open(
            "AUDIO\\PC\\SFX\\SFX.RAW",
            0x80000000u,
            1u,
            3u,
            0x60000000u));
    ok &= expect_true(
        "raw sfx open match is case-insensitive",
        mhmodern::audio_patch::detail::should_buffer_raw_audio_open(
            "audio\\pc\\sfx\\sfx.raw",
            0x80000000u,
            1u,
            3u,
            0x60000000u));
    ok &= expect_true(
        "raw sfx open requires no-buffering",
        !mhmodern::audio_patch::detail::should_buffer_raw_audio_open(
            "AUDIO\\PC\\SFX\\SFX.RAW",
            0x80000000u,
            1u,
            3u,
            0x40000000u));

    std::array<mhmodern::audio_patch::DriverOpenAttempt, 3> attempts{};
    std::size_t count = mhmodern::audio_patch::detail::build_driver_attempts(
        22050,
        8,
        1,
        48000,
        16,
        2,
        attempts);
    ok &= expect_true("count with canonical fallback", count == 3);
    ok &= expect_true(
        "requested first",
        attempts[0].sample_rate == 22050 && attempts[0].bits == 8 && attempts[0].channels == 1);
    ok &= expect_true(
        "configured fallback second",
        attempts[1].sample_rate == 48000 && attempts[1].bits == 16 && attempts[1].channels == 2);
    ok &= expect_true(
        "canonical stereo fallback third",
        attempts[2].sample_rate == 44100 && attempts[2].bits == 16 && attempts[2].channels == 2);

    attempts = {};
    count = mhmodern::audio_patch::detail::build_driver_attempts(
        44100,
        16,
        2,
        44100,
        16,
        2,
        attempts);
    ok &= expect_true("duplicates removed", count == 1);

    attempts = {};
    count = mhmodern::audio_patch::detail::build_driver_attempts(
        44100,
        16,
        2,
        48000,
        16,
        2,
        attempts);
    ok &= expect_true("request-first mode deduplicates canonical fallback", count == 2);
    ok &= expect_true(
        "requested format first",
        attempts[0].sample_rate == 44100 && attempts[0].bits == 16 && attempts[0].channels == 2);
    ok &= expect_true(
        "configured fallback second",
        attempts[1].sample_rate == 48000 && attempts[1].bits == 16 && attempts[1].channels == 2);

    if (!ok) {
        return 1;
    }

    std::cout << "audio_patch tests passed\n";
    return 0;
}
