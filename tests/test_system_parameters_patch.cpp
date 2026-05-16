#include "../src/system_parameters_patch.h"

#include <iostream>

namespace {

bool expect_equal(const char* label, bool actual, bool expected) {
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
        "suppress screensaver in bootstrap",
        mhmodern::system_parameters_patch::detail::should_suppress_spi_call(0x004C1100, SPI_SETSCREENSAVEACTIVE),
        true);
    ok &= expect_equal(
        "suppress low power in bootstrap",
        mhmodern::system_parameters_patch::detail::should_suppress_spi_call(0x004C1110, SPI_SETLOWPOWERACTIVE),
        true);
    ok &= expect_equal(
        "suppress power off in bootstrap",
        mhmodern::system_parameters_patch::detail::should_suppress_spi_call(0x004C1120, SPI_SETPOWEROFFACTIVE),
        true);
    ok &= expect_equal(
        "suppress sticky keys in bootstrap",
        mhmodern::system_parameters_patch::detail::should_suppress_spi_call(0x004C1130, SPI_SETSTICKYKEYS),
        true);
    ok &= expect_equal(
        "suppress foreground lock timeout in bootstrap",
        mhmodern::system_parameters_patch::detail::should_suppress_spi_call(0x004C1140, SPI_SETFOREGROUNDLOCKTIMEOUT),
        true);
    ok &= expect_equal(
        "do not suppress sticky keys outside bootstrap",
        mhmodern::system_parameters_patch::detail::should_suppress_spi_call(0x00500000, SPI_SETSTICKYKEYS),
        false);
    ok &= expect_equal(
        "do not suppress foreground lock timeout outside bootstrap",
        mhmodern::system_parameters_patch::detail::should_suppress_spi_call(0x00500000, SPI_SETFOREGROUNDLOCKTIMEOUT),
        false);
    ok &= expect_equal(
        "do not suppress same action outside bootstrap",
        mhmodern::system_parameters_patch::detail::should_suppress_spi_call(0x00500000, SPI_SETSCREENSAVEACTIVE),
        false);

    if (!ok) {
        return 1;
    }

    std::cout << "system_parameters_patch tests passed\n";
    return 0;
}
