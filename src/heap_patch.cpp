#include "heap_patch.h"

#include "logger.h"

#include <windows.h>

#include <vector>

namespace mhmodern::heap_patch::detail {

bool should_enable_lfh(HANDLE heap, HANDLE process_heap, bool low_fragmentation_heap_enabled) {
    return low_fragmentation_heap_enabled &&
           heap != nullptr &&
           heap == process_heap;
}

}  // namespace mhmodern::heap_patch::detail

namespace mhmodern::heap_patch {

bool install(const Settings& settings) {
    if (!settings.enabled) {
        return true;
    }

    bool terminate_success = true;
    if (settings.terminate_on_corruption) {
        terminate_success = HeapSetInformation(
            nullptr,
            HeapEnableTerminationOnCorruption,
            nullptr,
            0) == TRUE;
        logger::write(
            "Heap patch: terminationOnCorruption=%s error=%lu",
            terminate_success ? "yes" : "no",
            terminate_success ? 0ul : GetLastError());
    }

    DWORD heap_count = GetProcessHeaps(0, nullptr);
    std::vector<HANDLE> heaps(heap_count == 0 ? 1u : static_cast<std::size_t>(heap_count));
    if (heap_count != 0) {
        heap_count = GetProcessHeaps(heap_count, heaps.data());
    }

    std::size_t lfh_enabled_count = 0;
    const HANDLE process_heap = GetProcessHeap();
    ULONG lfh_mode = 2;

    for (DWORD index = 0; index < heap_count; ++index) {
        HANDLE heap = heaps[index];
        if (!detail::should_enable_lfh(heap, process_heap, settings.low_fragmentation_heap)) {
            continue;
        }

        if (HeapSetInformation(
                heap,
                HeapCompatibilityInformation,
                &lfh_mode,
                sizeof(lfh_mode)) == TRUE) {
            lfh_enabled_count += 1;
        }
    }

    logger::write(
        "Heap patch: heaps=%lu lfhEnabled=%llu lfhRequested=%d",
        static_cast<unsigned long>(heap_count),
        static_cast<unsigned long long>(lfh_enabled_count),
        settings.low_fragmentation_heap ? 1 : 0);
    return terminate_success;
}

}  // namespace mhmodern::heap_patch
