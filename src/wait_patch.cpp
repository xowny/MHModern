#include "wait_patch.h"

#include <windows.h>

#include <cstdint>

namespace {

constexpr std::uintptr_t kFlagAddress = 0x00756230;
constexpr std::uintptr_t kTimerBaseAddress = 0x00736DB8;
constexpr std::uintptr_t kTimerOriginAddress = 0x00756268;
constexpr std::uintptr_t kPreviousDeltaAddress = 0x008213D0;
constexpr std::uintptr_t kTimerMirrorSourceAddress = 0x008295B0;
constexpr std::uintptr_t kTimerMirrorTargetAddress = 0x008213CC;
constexpr std::uintptr_t kRenderContextAddress = 0x00715B9C;

using NoArgFn = void(__cdecl*)();
using ThiscallNoArgFn = void(__thiscall*)(void*);
using ThiscallTimeFn = std::uint32_t(__thiscall*)(void*);

void call_frame_reset_path() {
    auto reset_a = reinterpret_cast<NoArgFn>(0x004D83D0);
    auto reset_b = reinterpret_cast<NoArgFn>(0x004D8500);
    auto finalize = reinterpret_cast<ThiscallNoArgFn>(0x0045BD20);

    reset_a();
    reset_b();

    auto* context = *reinterpret_cast<void**>(kRenderContextAddress);
    finalize(context);
    *reinterpret_cast<std::uint8_t*>(kFlagAddress) = 0;
}

std::uint32_t query_game_timer() {
    auto* owner = *reinterpret_cast<void**>(kTimerBaseAddress);
    auto* timer = *reinterpret_cast<void**>(reinterpret_cast<std::uint8_t*>(owner) + 0x0c);
    auto** vtable = *reinterpret_cast<void***>(timer);
    auto fn = reinterpret_cast<ThiscallTimeFn>(vtable[7]);
    return fn(timer);
}

void precise_wait_until_ready() {
    *reinterpret_cast<std::uint32_t*>(kTimerMirrorTargetAddress) =
        *reinterpret_cast<std::uint32_t*>(kTimerMirrorSourceAddress);

    constexpr auto minimum_step = 5u;
    for (;;) {
        const auto current_delta = mhmodern::wait_patch::wrapped_elapsed(
            *reinterpret_cast<std::uint32_t*>(kTimerOriginAddress),
            query_game_timer());
        const auto previous_delta = *reinterpret_cast<std::uint32_t*>(kPreviousDeltaAddress);
        const auto remaining = mhmodern::wait_patch::remaining_delay(previous_delta, current_delta, minimum_step);
        if (remaining == 0) {
            *reinterpret_cast<std::uint32_t*>(kPreviousDeltaAddress) = current_delta;
            return;
        }

        if (remaining > 1) {
            Sleep(remaining - 1);
        } else {
            SwitchToThread();
        }
    }
}

}  // namespace

namespace mhmodern::wait_patch {

std::uint32_t wrapped_elapsed(std::uint32_t base, std::uint32_t now) {
    return now >= base ? now - base : now + (0xffffffffu - base);
}

std::uint32_t remaining_delay(std::uint32_t previous_delta, std::uint32_t current_delta, std::uint32_t minimum_step) {
    if (current_delta < previous_delta) {
        return 0;
    }

    const auto elapsed_since_previous = current_delta - previous_delta;
    if (elapsed_since_previous >= minimum_step) {
        return 0;
    }

    return minimum_step - elapsed_since_previous;
}

void __cdecl hooked_wait_routine() {
    if (*reinterpret_cast<std::uint8_t*>(kFlagAddress) != 0) {
        call_frame_reset_path();
        return;
    }

    precise_wait_until_ready();
}

}  // namespace mhmodern::wait_patch
