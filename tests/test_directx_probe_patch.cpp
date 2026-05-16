#include "../src/directx_probe_patch.h"

#include <iostream>

namespace {

bool expect_equal(const char* label, const std::uint32_t actual, const std::uint32_t expected) {
    if (actual != expected) {
        std::cerr << label << " failed: expected " << expected << ", got " << actual << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    ok &= expect_equal(
        "all present returns 0x900",
        mhmodern::directx_probe_patch::detail::compute_probe_result(true, true, true, true, true),
        0x900);
    ok &= expect_equal(
        "dpnhpast optional",
        mhmodern::directx_probe_patch::detail::compute_probe_result(true, true, true, false, true),
        0x900);
    ok &= expect_equal(
        "missing d3d8 returns 0x700",
        mhmodern::directx_probe_patch::detail::compute_probe_result(true, true, false, true, true),
        0x700);
    ok &= expect_equal(
        "missing d3d9 returns 0x801",
        mhmodern::directx_probe_patch::detail::compute_probe_result(true, true, true, true, false),
        0x801);
    ok &= expect_equal(
        "missing ddraw returns 0",
        mhmodern::directx_probe_patch::detail::compute_probe_result(false, true, true, true, true),
        0);
    ok &= expect_equal(
        "missing proc returns 0",
        mhmodern::directx_probe_patch::detail::compute_probe_result(true, false, true, true, true),
        0);

    if (!ok) {
        return 1;
    }

    std::cout << "directx_probe_patch tests passed\n";
    return 0;
}
