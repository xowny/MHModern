#include "../src/version_patch.h"

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

    const DWORD packed = mhmodern::version_patch::detail::pack_get_version_result(10, 0, 19045);
    ok &= expect_true("major in low byte", (packed & 0xFFu) == 10u);
    ok &= expect_true("minor in second byte", ((packed >> 8) & 0xFFu) == 0u);
    ok &= expect_true("build in hi word", ((packed >> 16) & 0x7FFFu) == 19045u);

    if (!ok) {
        return 1;
    }

    std::cout << "version_patch tests passed\n";
    return 0;
}
