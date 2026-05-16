#include "../src/event_patch.h"

#include <iostream>

namespace {

bool expect_true(const char* label, bool value) {
    if (!value) {
        std::cerr << label << " failed\n";
        return false;
    }
    return true;
}

bool expect_false(const char* label, bool value) {
    if (value) {
        std::cerr << label << " failed\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    ok &= expect_true(
        "audio temp event caller A pooled",
        mhmodern::event_patch::detail::should_pool_audio_temp_event(
            0x00458575,
            nullptr,
            FALSE,
            FALSE,
            nullptr));
    ok &= expect_true(
        "audio temp event caller B pooled",
        mhmodern::event_patch::detail::should_pool_audio_temp_event(
            0x00459F0A,
            nullptr,
            FALSE,
            FALSE,
            nullptr));
    ok &= expect_false(
        "manual reset event not pooled",
        mhmodern::event_patch::detail::should_pool_audio_temp_event(
            0x00458575,
            nullptr,
            TRUE,
            FALSE,
            nullptr));
    ok &= expect_false(
        "named event not pooled",
        mhmodern::event_patch::detail::should_pool_audio_temp_event(
            0x00458575,
            nullptr,
            FALSE,
            FALSE,
            "named"));
    ok &= expect_false(
        "other caller not pooled",
        mhmodern::event_patch::detail::should_pool_audio_temp_event(
            0x00456D0D,
            nullptr,
            FALSE,
            FALSE,
            nullptr));

    if (!ok) {
        return 1;
    }

    std::cout << "event_patch tests passed\n";
    return 0;
}
