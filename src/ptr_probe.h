#pragma once

#include <windows.h>

#include <cstddef>

namespace mhmodern::ptr_probe {

bool is_readable_protection(DWORD protect);
bool is_writable_protection(DWORD protect);
bool is_executable_protection(DWORD protect);
bool can_access_range(const void* address, std::size_t size, bool require_write, bool require_execute);

}  // namespace mhmodern::ptr_probe
