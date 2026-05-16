#include <windows.h>

#include <iostream>

namespace mhmodern::ptr_probe {
bool is_readable_protection(DWORD protect);
bool is_writable_protection(DWORD protect);
bool is_executable_protection(DWORD protect);
bool can_access_range(const void* address, std::size_t size, bool require_write, bool require_execute);
}

namespace {

bool expect_true(const char* label, bool value) {
    if (!value) {
        std::cerr << label << " failed: expected true\n";
        return false;
    }
    return true;
}

bool expect_false(const char* label, bool value) {
    if (value) {
        std::cerr << label << " failed: expected false\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    ok &= expect_true("readable readonly", mhmodern::ptr_probe::is_readable_protection(PAGE_READONLY));
    ok &= expect_false("writable readonly", mhmodern::ptr_probe::is_writable_protection(PAGE_READONLY));
    ok &= expect_true("executable execute_read", mhmodern::ptr_probe::is_executable_protection(PAGE_EXECUTE_READ));

    void* page = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (page == nullptr) {
        std::cerr << "VirtualAlloc failed\n";
        return 1;
    }

    ok &= expect_true("access readable committed", mhmodern::ptr_probe::can_access_range(page, 16, false, false));
    ok &= expect_true("access writable committed", mhmodern::ptr_probe::can_access_range(page, 16, true, false));
    ok &= expect_false("access executable writable page", mhmodern::ptr_probe::can_access_range(page, 16, false, true));

    DWORD old_protect = 0;
    if (!VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &old_protect)) {
        std::cerr << "VirtualProtect failed\n";
        VirtualFree(page, 0, MEM_RELEASE);
        return 1;
    }

    ok &= expect_true("access executable execute_read page", mhmodern::ptr_probe::can_access_range(page, 16, false, true));
    ok &= expect_false("access writable execute_read page", mhmodern::ptr_probe::can_access_range(page, 16, true, false));

    VirtualFree(page, 0, MEM_RELEASE);

    if (!ok) {
        return 1;
    }

    std::cout << "ptr_probe tests passed\n";
    return 0;
}
