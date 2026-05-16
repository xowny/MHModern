#include "../src/input_patch.h"

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>

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
        "input lost retries",
        mhmodern::input_patch::detail::should_retry_input_hresult(DIERR_INPUTLOST));
    ok &= expect_true(
        "not acquired retries",
        mhmodern::input_patch::detail::should_retry_input_hresult(DIERR_NOTACQUIRED));
    ok &= expect_false(
        "other app has priority does not retry",
        mhmodern::input_patch::detail::should_retry_input_hresult(DIERR_OTHERAPPHASPRIO));
    ok &= expect_false(
        "success does not retry",
        mhmodern::input_patch::detail::should_retry_input_hresult(DI_OK));
    int dummy = 0;
    ok &= expect_true(
        "valid get device state request retries",
        mhmodern::input_patch::detail::should_retry_get_device_state_request(
            DIERR_INPUTLOST, sizeof(dummy), &dummy));
    ok &= expect_false(
        "zero-byte get device state request does not retry",
        mhmodern::input_patch::detail::should_retry_get_device_state_request(
            DIERR_INPUTLOST, 0, &dummy));
    ok &= expect_false(
        "null-buffer get device state request does not retry",
        mhmodern::input_patch::detail::should_retry_get_device_state_request(
            DIERR_INPUTLOST, sizeof(dummy), nullptr));
    ok &= expect_true(
        "system mouse guid wraps",
        mhmodern::input_patch::detail::should_wrap_mouse_guid(GUID_SysMouse));

    const auto raw_delta = mhmodern::input_patch::detail::choose_mouse_delta(true, 17, -9, 3, 4);
    ok &= expect_true("raw mouse x overrides fallback", raw_delta.x == 17);
    ok &= expect_true("raw mouse y overrides fallback", raw_delta.y == -9);

    const auto fallback_delta =
        mhmodern::input_patch::detail::choose_mouse_delta(false, 17, -9, 3, 4);
    ok &= expect_true("fallback mouse x preserved", fallback_delta.x == 3);
    ok &= expect_true("fallback mouse y preserved", fallback_delta.y == 4);

    ok &= expect_true(
        "raw mouse saturation clamps high",
        mhmodern::input_patch::detail::saturating_long_from_delta(
            static_cast<std::int64_t>(LONG_MAX) + 99) == LONG_MAX);
    ok &= expect_true(
        "raw mouse saturation clamps low",
        mhmodern::input_patch::detail::saturating_long_from_delta(
            static_cast<std::int64_t>(LONG_MIN) - 99) == LONG_MIN);

    if (!ok) {
        return 1;
    }

    std::cout << "input_patch tests passed\n";
    return 0;
}
