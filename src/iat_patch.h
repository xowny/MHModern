#pragma once

#include <windows.h>

namespace mhmodern {

bool patch_iat(HMODULE module,
               const char* imported_module_name,
               const char* function_name,
               void* replacement,
               void** original);

}  // namespace mhmodern
