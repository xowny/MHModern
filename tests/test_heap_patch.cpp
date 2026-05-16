#include "../src/heap_patch.h"

#include <iostream>

namespace {

bool expect_true(const char* label, bool value) {
    if (!value) {
        std::cerr << label << " failed\n";
        return false;
    }
    return true;
}

bool expect_false(const char* label, bool value) {
    if (value) {
        std::cerr << label << " failed\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    HANDLE fake_heap = reinterpret_cast<HANDLE>(0x1234);
    HANDLE process_heap = reinterpret_cast<HANDLE>(0x1234);
    HANDLE other_heap = reinterpret_cast<HANDLE>(0x5678);

    ok &= expect_true(
        "lfh enabled on process heap",
        mhmodern::heap_patch::detail::should_enable_lfh(fake_heap, process_heap, true));
    ok &= expect_false(
        "lfh disabled by setting",
        mhmodern::heap_patch::detail::should_enable_lfh(fake_heap, process_heap, false));
    ok &= expect_false(
        "lfh not enabled on non-process heap",
        mhmodern::heap_patch::detail::should_enable_lfh(other_heap, process_heap, true));
    ok &= expect_false(
        "lfh not enabled on null heap",
        mhmodern::heap_patch::detail::should_enable_lfh(nullptr, process_heap, true));

    if (!ok) {
        return 1;
    }

    std::cout << "heap_patch tests passed\n";
    return 0;
}
