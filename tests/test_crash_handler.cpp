#include "../src/crash_handler.h"

#include <DbgHelp.h>

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

    ok &= expect_true(
        "sanitize invalid characters",
        mhmodern::crash_handler::detail::sanitize_component("manhunt: crash? test") == "manhunt__crash__test");
    ok &= expect_true(
        "sanitize empty",
        mhmodern::crash_handler::detail::sanitize_component("") == "unknown");

    SYSTEMTIME sample{};
    sample.wYear = 2026;
    sample.wMonth = 4;
    sample.wDay = 3;
    sample.wHour = 18;
    sample.wMinute = 30;
    sample.wSecond = 45;
    sample.wMilliseconds = 123;
    ok &= expect_true(
        "build filename",
        mhmodern::crash_handler::detail::build_dump_filename("manhunt.exe", sample) ==
            "manhunt.exe_20260403_183045_123.dmp");

    const auto normal_type = mhmodern::crash_handler::detail::select_dump_type(false);
    const auto full_type = mhmodern::crash_handler::detail::select_dump_type(true);
    ok &= expect_true("normal includes MiniDumpNormal", (normal_type & MiniDumpNormal) == MiniDumpNormal);
    ok &= expect_true(
        "full includes MiniDumpWithFullMemory",
        (full_type & MiniDumpWithFullMemory) == MiniDumpWithFullMemory);

    if (!ok) {
        return 1;
    }

    std::cout << "crash_handler tests passed\n";
    return 0;
}
