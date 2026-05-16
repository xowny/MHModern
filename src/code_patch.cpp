#include "code_patch.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

bool write_relative_branch(
    void* target,
    const void* destination,
    const std::size_t patch_size,
    const std::uint8_t opcode,
    const bool require_call_opcode,
    const bool pad_with_nops) {
    if (target == nullptr || destination == nullptr || patch_size < 5) {
        return false;
    }

    auto* bytes = static_cast<std::uint8_t*>(target);
    if (require_call_opcode && bytes[0] != 0xE8) {
        return false;
    }

    const auto source_address = reinterpret_cast<std::uintptr_t>(target);
    const auto destination_address = reinterpret_cast<std::uintptr_t>(destination);
    const auto relative_offset_wide =
        static_cast<std::int64_t>(destination_address) - static_cast<std::int64_t>(source_address + 5);
    constexpr std::int64_t kMinRelativeOffset = -0x80000000LL;
    constexpr std::int64_t kMaxRelativeOffset = 0x7FFFFFFFLL;
    if (relative_offset_wide < kMinRelativeOffset || relative_offset_wide > kMaxRelativeOffset) {
        return false;
    }

    DWORD old_protection = 0;
    if (!VirtualProtect(target, patch_size, PAGE_EXECUTE_READWRITE, &old_protection)) {
        return false;
    }

    const auto relative_offset = static_cast<std::int32_t>(relative_offset_wide);
    bytes[0] = opcode;
    std::memcpy(bytes + 1, &relative_offset, sizeof(relative_offset));
    if (pad_with_nops && patch_size > 5) {
        std::fill(bytes + 5, bytes + patch_size, static_cast<std::uint8_t>(0x90));
    }

    DWORD ignored = 0;
    VirtualProtect(target, patch_size, old_protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, patch_size);
    return true;
}

}  // namespace

namespace mhmodern {

bool patch_jmp(void* target, const void* destination, const std::size_t patch_size) {
    return write_relative_branch(target, destination, patch_size, 0xE9, false, true);
}

bool patch_call(void* target, const void* destination) {
    return write_relative_branch(target, destination, 5, 0xE8, true, false);
}

}  // namespace mhmodern
