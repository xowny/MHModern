#include "../src/telemetry.h"

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
    mhmodern::telemetry::Stats stats{};
    mhmodern::telemetry::detail::update_stats(stats, 16);
    mhmodern::telemetry::detail::update_stats(stats, 17);
    mhmodern::telemetry::detail::update_stats(stats, 15);

    bool ok = true;
    ok &= expect_true("count", stats.count == 3);
    ok &= expect_true("total", stats.total_delta_ms == 48);
    ok &= expect_true("min", stats.min_delta_ms == 15);
    ok &= expect_true("max", stats.max_delta_ms == 17);
    ok &= expect_true("avg", mhmodern::telemetry::detail::average_delta_ms(stats) > 15.9 &&
        mhmodern::telemetry::detail::average_delta_ms(stats) < 16.1);

    mhmodern::telemetry::CallsiteStat callsites[3]{};
    mhmodern::telemetry::detail::update_callsite_stats(callsites, 3, 0x1000);
    mhmodern::telemetry::detail::update_callsite_stats(callsites, 3, 0x2000);
    mhmodern::telemetry::detail::update_callsite_stats(callsites, 3, 0x1000);
    ok &= expect_true("callsite first count", callsites[0].address == 0x1000 && callsites[0].count == 2);
    ok &= expect_true("callsite second count", callsites[1].address == 0x2000 && callsites[1].count == 1);
    ok &= expect_true(
        "basename windows path",
        mhmodern::telemetry::detail::basename_from_path("C:\\Games\\Manhunt\\dinput8.dll") == "dinput8.dll");
    ok &= expect_true(
        "basename plain name",
        mhmodern::telemetry::detail::basename_from_path("manhunt.exe") == "manhunt.exe");

    if (!ok) {
        return 1;
    }

    std::cout << "telemetry tests passed\n";
    return 0;
}
