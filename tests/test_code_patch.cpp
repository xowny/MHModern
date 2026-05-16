#include "../src/code_patch.h"

#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

bool expect_true(const char* label, const bool value) {
    if (!value) {
        std::cerr << label << " failed\n";
        return false;
    }
    return true;
}

bool expect_equal_byte(const char* label, const std::uint8_t actual, const std::uint8_t expected) {
    if (actual != expected) {
        std::cerr << label << " failed: actual=" << static_cast<unsigned>(actual)
                  << " expected=" << static_cast<unsigned>(expected) << '\n';
        return false;
    }
    return true;
}

bool expect_equal_i32(const char* label, const std::int32_t actual, const std::int32_t expected) {
    if (actual != expected) {
        std::cerr << label << " failed: actual=" << actual << " expected=" << expected << '\n';
        return false;
    }
    return true;
}

void __stdcall destination_stub() {
}

}  // namespace

int main() {
    bool ok = true;

    alignas(16) std::uint8_t call_bytes[8] = {0xE8, 0x11, 0x22, 0x33, 0x44, 0xAA, 0xBB, 0xCC};
    ok &= expect_true("patch_call succeeds", mhmodern::patch_call(call_bytes, reinterpret_cast<const void*>(&destination_stub)));
    ok &= expect_equal_byte("opcode preserved as call", call_bytes[0], 0xE8);

    std::int32_t expected_relative = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(&destination_stub) - (reinterpret_cast<std::uintptr_t>(call_bytes) + 5));
    std::int32_t actual_relative = 0;
    std::memcpy(&actual_relative, call_bytes + 1, sizeof(actual_relative));
    ok &= expect_equal_i32("relative offset updated", actual_relative, expected_relative);
    ok &= expect_equal_byte("tail byte 5 untouched", call_bytes[5], 0xAA);
    ok &= expect_equal_byte("tail byte 6 untouched", call_bytes[6], 0xBB);
    ok &= expect_equal_byte("tail byte 7 untouched", call_bytes[7], 0xCC);

    std::uint8_t not_call_bytes[5] = {0x90, 0x11, 0x22, 0x33, 0x44};
    ok &= expect_true("patch_call rejects non-call opcode", !mhmodern::patch_call(not_call_bytes, reinterpret_cast<const void*>(&destination_stub)));
    ok &= expect_equal_byte("non-call opcode unchanged", not_call_bytes[0], 0x90);

    if (!ok) {
        return 1;
    }

    std::cout << "code_patch tests passed\n";
    return 0;
}
