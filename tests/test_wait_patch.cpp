#include <cstdint>
#include <iostream>

namespace mhmodern::wait_patch {
std::uint32_t wrapped_elapsed(std::uint32_t base, std::uint32_t now);
std::uint32_t remaining_delay(std::uint32_t previous_delta, std::uint32_t current_delta, std::uint32_t minimum_step);
}

namespace {

bool expect_equal(const char* label, std::uint32_t actual, std::uint32_t expected) {
    if (actual != expected) {
        std::cerr << label << " failed: expected " << expected << ", got " << actual << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    ok &= expect_equal("wrapped_elapsed basic", mhmodern::wait_patch::wrapped_elapsed(100, 109), 9);
    ok &= expect_equal("wrapped_elapsed wrap", mhmodern::wait_patch::wrapped_elapsed(0xfffffff0u, 5u), 20u);
    ok &= expect_equal("remaining_delay none", mhmodern::wait_patch::remaining_delay(10, 15, 5), 0);
    ok &= expect_equal("remaining_delay one", mhmodern::wait_patch::remaining_delay(10, 14, 5), 1);
    ok &= expect_equal("remaining_delay full", mhmodern::wait_patch::remaining_delay(10, 10, 5), 5);
    ok &= expect_equal("remaining_delay backwards", mhmodern::wait_patch::remaining_delay(20, 10, 5), 0);

    if (!ok) {
        return 1;
    }

    std::cout << "wait_patch tests passed\n";
    return 0;
}
