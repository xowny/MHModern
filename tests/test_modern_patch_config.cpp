#include "../src/modern_patch_internal.h"

#include <fstream>
#include <filesystem>
#include <iostream>

namespace {

bool expect_true(const char* label, bool value) {
    if (!value) {
        std::cerr << label << " failed\n";
        return false;
    }
    return true;
}

bool expect_eq(const char* label, const auto& lhs, const auto& rhs) {
    if (lhs != rhs) {
        std::cerr << label << " failed\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    const auto temp_ini = std::filesystem::temp_directory_path() / "mhm-modern-patch-config-test.ini";
    {
        std::ofstream out(temp_ini, std::ios::trunc);
        out <<
            "[Main]\n"
            "DetectMilesPrimaryFormat=0\n"
            "EnableEventPatch=1\n"
            "MilesFallbackRate=44100\n"
            "CrashDumpDirectory=CustomDumps\n";
    }

    const auto settings = mhmodern::modern_patch::detail::load_settings_from(temp_ini);

    bool ok = true;
    ok &= expect_true("miles audio patch defaults to on", settings.miles_audio_patch);
    ok &= expect_true("detect miles primary format override", !settings.detect_miles_primary_format);
    ok &= expect_true("event patch override", settings.event_patch);
    ok &= expect_true("pool audio temp events default off", !settings.pool_audio_temp_events);
    ok &= expect_true("heap patch default off", !settings.heap_patch);
    ok &= expect_true("heap terminate on corruption default on", settings.heap_terminate_on_corruption);
    ok &= expect_true("miles fallback rate override", settings.miles_fallback_rate == 44100u);
    ok &= expect_true("miles fallback bits default", settings.miles_fallback_bits == 16);
    ok &= expect_true("miles fallback channels default", settings.miles_fallback_channels == 2);
    ok &= expect_true("raw audio buffered open default on", settings.raw_audio_buffered_open);
    ok &= expect_eq("crash dump dir override", settings.crash_dump_directory, std::string("CustomDumps"));

    std::error_code ignored;
    std::filesystem::remove(temp_ini, ignored);

    if (!ok) {
        return 1;
    }

    std::cout << "modern_patch_config tests passed\n";
    return 0;
}
