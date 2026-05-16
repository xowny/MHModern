#pragma once

#include <windows.h>

#include <cstddef>

namespace mhmodern {

bool patch_jmp(void* target, const void* destination, std::size_t patch_size);
bool patch_call(void* target, const void* destination);

}  // namespace mhmodern
