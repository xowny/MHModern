#include "../src/error_mode_patch.h"

#include <iostream>

namespace {

bool expect_equal(const char* label, UINT actual, UINT expected) {
    if (actual != expected) {
        std::cerr << label << " failed: expected " << expected << ", got " << actual << '\n';
        return false;
    }
    return true;
}

bool expect_true(const char* label, bool actual) {
    if (!actual) {
        std::cerr << label << " failed\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    ok &= expect_true(
        "bootstrap caller recognized",
        mhmodern::error_mode_patch::detail::is_bootstrap_caller(0x004C1100));
    ok &= expect_equal(
        "bootstrap restore remaps to saved mode",
        mhmodern::error_mode_patch::detail::translate_error_mode_request(0x004C1120, 0, true, 7),
        7);
    ok &= expect_equal(
        "bootstrap set failcritical unchanged",
        mhmodern::error_mode_patch::detail::translate_error_mode_request(
            0x004C1120, SEM_FAILCRITICALERRORS, true, 7),
        SEM_FAILCRITICALERRORS);
    ok &= expect_equal(
        "outside bootstrap not remapped",
        mhmodern::error_mode_patch::detail::translate_error_mode_request(0x00500000, 0, true, 7),
        0);

    if (!ok) {
        return 1;
    }

    std::cout << "error_mode_patch tests passed\n";
    return 0;
}
