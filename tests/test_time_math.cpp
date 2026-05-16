#include <cstdint>
#include <iostream>
#include <limits>

namespace mhmodern::time_math {
uint64_t counter_to_milliseconds(int64_t counter_delta, int64_t frequency);
uint32_t wrap_milliseconds(uint64_t milliseconds);
uint32_t lower_32_bits(uint64_t value);
uint32_t upper_32_bits(uint64_t value);
}

namespace {

bool expect_equal(const char* label, uint64_t actual, uint64_t expected) {
    if (actual == expected) {
        return true;
    }

    std::cerr << label << " failed: expected " << expected << ", got " << actual << '\n';
    return false;
}

}  // namespace

int main() {
    bool ok = true;

    ok &= expect_equal("counter_to_milliseconds basic",
        mhmodern::time_math::counter_to_milliseconds(12'000, 1'000), 12'000);
    ok &= expect_equal("counter_to_milliseconds truncates toward zero",
        mhmodern::time_math::counter_to_milliseconds(2'999, 3'000), 999);
    ok &= expect_equal("counter_to_milliseconds avoids overflow before divide",
        mhmodern::time_math::counter_to_milliseconds(9'223'372'036'854'000'000ll, 1'000),
        9'223'372'036'854'000'000ull);
    ok &= expect_equal("wrap_milliseconds zero",
        mhmodern::time_math::wrap_milliseconds(0), 0);
    ok &= expect_equal("wrap_milliseconds wrap",
        mhmodern::time_math::wrap_milliseconds(static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 42ull), 41);
    ok &= expect_equal("lower_32_bits",
        mhmodern::time_math::lower_32_bits(0x1122334455667788ull), 0x55667788ull);
    ok &= expect_equal("upper_32_bits",
        mhmodern::time_math::upper_32_bits(0x1122334455667788ull), 0x11223344ull);

    if (!ok) {
        return 1;
    }

    std::cout << "time_math tests passed\n";
    return 0;
}
