#include "ptr_probe.h"

#include <cstdint>

namespace {

constexpr DWORD kReadMask =
    PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
    PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

constexpr DWORD kWriteMask =
    PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

constexpr DWORD kExecuteMask =
    PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

DWORD effective_protect(const MEMORY_BASIC_INFORMATION& mbi) {
    return mbi.Protect != 0 ? mbi.Protect : mbi.AllocationProtect;
}

bool is_access_blocked(const MEMORY_BASIC_INFORMATION& mbi) {
    const DWORD protect = effective_protect(mbi);
    return mbi.State != MEM_COMMIT || (protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0;
}

}  // namespace

namespace mhmodern::ptr_probe {

bool is_readable_protection(DWORD protect) {
    return (protect & kReadMask) != 0;
}

bool is_writable_protection(DWORD protect) {
    return (protect & kWriteMask) != 0;
}

bool is_executable_protection(DWORD protect) {
    return (protect & kExecuteMask) != 0;
}

bool can_access_range(const void* address, std::size_t size, bool require_write, bool require_execute) {
    if (address == nullptr) {
        return false;
    }

    if (size == 0) {
        return true;
    }

    auto current = reinterpret_cast<std::uintptr_t>(address);
    const auto end = current + size;
    if (end < current) {
        return false;
    }

    while (current < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(current), &mbi, sizeof(mbi)) != sizeof(mbi)) {
            return false;
        }

        if (is_access_blocked(mbi)) {
            return false;
        }

        const DWORD protect = effective_protect(mbi);
        if (require_execute) {
            if (!is_executable_protection(protect)) {
                return false;
            }
        } else if (require_write) {
            if (!is_writable_protection(protect)) {
                return false;
            }
        } else {
            if (!is_readable_protection(protect)) {
                return false;
            }
        }

        const auto region_base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto region_end = region_base + mbi.RegionSize;
        if (region_end <= current) {
            return false;
        }

        current = region_end < end ? region_end : end;
    }

    return true;
}

}  // namespace mhmodern::ptr_probe
