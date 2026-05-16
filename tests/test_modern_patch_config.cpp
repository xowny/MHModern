#include "../src/modern_patch_internal.h"

#include <windows.h>

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
            "EnableRawMouseInput=0\n"
            "EnablePowerThrottlingOptOut=0\n"
            "MilesFallbackRate=44100\n"
            "MilesFallbackBits=24\n"
            "MilesFallbackChannels=1\n"
            "TimingTelemetryFlushMs=2500\n"
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
    ok &= expect_true("raw mouse override", !settings.raw_mouse_input);
    ok &= expect_true("power throttling opt-out override", !settings.power_throttling_opt_out);
    ok &= expect_true("miles fallback rate override", settings.miles_fallback_rate == 44100u);
    ok &= expect_true("miles fallback bits override", settings.miles_fallback_bits == 24);
    ok &= expect_true("miles fallback channels override", settings.miles_fallback_channels == 1);
    ok &= expect_true("telemetry flush override", settings.telemetry_flush_interval_ms == 2500u);
    ok &= expect_true("raw audio buffered open default on", settings.raw_audio_buffered_open);
    ok &= expect_true(
        "power throttling execution-speed mask",
        mhmodern::modern_patch::detail::execution_speed_throttling_mask(true) == 1u);
    ok &= expect_true(
        "power throttling mask clears when disabled",
        mhmodern::modern_patch::detail::execution_speed_throttling_mask(false) == 0u);
    ok &= expect_true(
        "power throttling invalid function is unsupported",
        mhmodern::modern_patch::detail::is_power_throttling_unsupported_error(ERROR_INVALID_FUNCTION));
    ok &= expect_true(
        "power throttling not supported is unsupported",
        mhmodern::modern_patch::detail::is_power_throttling_unsupported_error(ERROR_NOT_SUPPORTED));
    ok &= expect_true(
        "power throttling access denied is not unsupported",
        !mhmodern::modern_patch::detail::is_power_throttling_unsupported_error(ERROR_ACCESS_DENIED));
    ok &= expect_eq("crash dump dir override", settings.crash_dump_directory, std::string("CustomDumps"));

    {
        std::ofstream out(temp_ini, std::ios::trunc);
        out <<
            "[Main]\n"
            "MilesFallbackRate=-1\n"
            "MilesFallbackBits=12\n"
            "MilesFallbackChannels=0\n"
            "TimingTelemetryFlushMs=0\n";
    }

    const auto sanitized = mhmodern::modern_patch::detail::load_settings_from(temp_ini);
    ok &= expect_true("miles fallback rate invalid resets to default", sanitized.miles_fallback_rate == 48000u);
    ok &= expect_true("miles fallback bits invalid reset to default", sanitized.miles_fallback_bits == 16);
    ok &= expect_true(
        "miles fallback channels invalid reset to default",
        sanitized.miles_fallback_channels == 2);
    ok &= expect_true(
        "telemetry flush invalid resets to default",
        sanitized.telemetry_flush_interval_ms == 5000u);

    std::error_code ignored;
    std::filesystem::remove(temp_ini, ignored);

    if (!ok) {
        return 1;
    }

    std::cout << "modern_patch_config tests passed\n";
    return 0;
}
