#include "iat_patch.h"

#include <cstring>

namespace {

bool equals_ignore_case(const char* lhs, const char* rhs) {
    return _stricmp(lhs, rhs) == 0;
}

}  // namespace

namespace mhmodern {

bool patch_iat(HMODULE module,
               const char* imported_module_name,
               const char* function_name,
               void* replacement,
               void** original) {
    if (module == nullptr || imported_module_name == nullptr || function_name == nullptr || replacement == nullptr) {
        return false;
    }

    auto* dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    auto* nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(
        reinterpret_cast<BYTE*>(module) + dos_header->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    const auto& import_directory =
        nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (import_directory.VirtualAddress == 0) {
        return false;
    }

    auto* import_descriptor = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
        reinterpret_cast<BYTE*>(module) + import_directory.VirtualAddress);

    for (; import_descriptor->Name != 0; ++import_descriptor) {
        auto* current_module_name = reinterpret_cast<const char*>(
            reinterpret_cast<BYTE*>(module) + import_descriptor->Name);
        if (!equals_ignore_case(current_module_name, imported_module_name)) {
            continue;
        }

        auto* thunk_data = reinterpret_cast<PIMAGE_THUNK_DATA>(
            reinterpret_cast<BYTE*>(module) + import_descriptor->FirstThunk);
        auto* lookup_data = import_descriptor->OriginalFirstThunk != 0
            ? reinterpret_cast<PIMAGE_THUNK_DATA>(
                  reinterpret_cast<BYTE*>(module) + import_descriptor->OriginalFirstThunk)
            : thunk_data;

        for (; lookup_data->u1.AddressOfData != 0; ++lookup_data, ++thunk_data) {
            if (IMAGE_SNAP_BY_ORDINAL(lookup_data->u1.Ordinal)) {
                continue;
            }

            auto* import_by_name = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
                reinterpret_cast<BYTE*>(module) + lookup_data->u1.AddressOfData);
            if (!equals_ignore_case(reinterpret_cast<const char*>(import_by_name->Name), function_name)) {
                continue;
            }

            DWORD old_protection = 0;
            if (!VirtualProtect(&thunk_data->u1.Function, sizeof(void*), PAGE_READWRITE, &old_protection)) {
                return false;
            }

            if (original != nullptr) {
                *original = reinterpret_cast<void*>(thunk_data->u1.Function);
            }

            thunk_data->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);

            DWORD ignored = 0;
            VirtualProtect(&thunk_data->u1.Function, sizeof(void*), old_protection, &ignored);
            FlushInstructionCache(GetCurrentProcess(), &thunk_data->u1.Function, sizeof(void*));
            return true;
        }
    }

    return false;
}

}  // namespace mhmodern
