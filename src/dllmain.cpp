#include "modern_patch.h"

#include <windows.h>

BOOL APIENTRY DllMain(HMODULE module_handle, DWORD reason, LPVOID reserved) {
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module_handle);
        HANDLE thread = CreateThread(
            nullptr,
            0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(mhmodern::initialize_mod),
            module_handle,
            0,
            nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }

    return TRUE;
}
