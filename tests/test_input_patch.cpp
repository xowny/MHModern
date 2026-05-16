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

    if (!ok) {
        return 1;
    }

    std::cout << "input_patch tests passed\n";
    return 0;
}
