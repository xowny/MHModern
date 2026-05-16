#include "crash_handler.h"

#include "logger.h"

#include <DbgHelp.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {

std::filesystem::path g_dump_directory;
LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter = nullptr;
std::atomic<long> g_dump_in_progress = 0;
bool g_full_memory_dumps = false;

std::filesystem::path exe_directory() {
    char buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) {
        return std::filesystem::current_path();
    }

    return std::filesystem::path(buffer).parent_path();
}

std::string process_basename() {
    char buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) {
        return "manhunt";
    }

    return std::filesystem::path(buffer).stem().string();
}

LONG WINAPI top_level_exception_filter(EXCEPTION_POINTERS* exception_pointers) {
    if (exception_pointers == nullptr || g_dump_directory.empty()) {
        return g_previous_filter != nullptr
            ? g_previous_filter(exception_pointers)
            : EXCEPTION_CONTINUE_SEARCH;
    }

    const long already_writing = g_dump_in_progress.exchange(1);
    if (already_writing == 0) {
        SYSTEMTIME local_time{};
        GetLocalTime(&local_time);

        const std::string dump_name = mhmodern::crash_handler::detail::build_dump_filename(
            process_basename(),
            local_time);
        const std::filesystem::path dump_path = g_dump_directory / dump_name;

        const HANDLE dump_file = CreateFileA(
            dump_path.string().c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (dump_file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION exception_info{};
            exception_info.ThreadId = GetCurrentThreadId();
            exception_info.ExceptionPointers = exception_pointers;
            exception_info.ClientPointers = FALSE;

            const BOOL wrote_dump = MiniDumpWriteDump(
                GetCurrentProcess(),
                GetCurrentProcessId(),
                dump_file,
                mhmodern::crash_handler::detail::select_dump_type(g_full_memory_dumps),
                &exception_info,
                nullptr,
                nullptr);

            if (wrote_dump == TRUE) {
                mhmodern::logger::write("Crash dump written: %s", dump_path.string().c_str());
            } else {
                mhmodern::logger::write(
                    "Crash dump failed: path=%s error=%lu",
                    dump_path.string().c_str(),
                    GetLastError());
            }

            CloseHandle(dump_file);
        } else {
            mhmodern::logger::write(
                "Crash dump file create failed: path=%s error=%lu",
                dump_path.string().c_str(),
                GetLastError());
        }
    }

    return g_previous_filter != nullptr
        ? g_previous_filter(exception_pointers)
        : EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

namespace mhmodern::crash_handler::detail {

std::string sanitize_component(std::string_view value) {
    std::string sanitized;
    sanitized.reserve(value.size());

    for (const char character : value) {
        switch (character) {
            case '<':
            case '>':
            case ':':
            case '"':
            case '/':
            case '\\':
            case '|':
            case '?':
            case '*':
            case ' ':
                sanitized.push_back('_');
                break;
            default:
                sanitized.push_back(character);
                break;
        }
    }

    while (!sanitized.empty() && sanitized.back() == '.') {
        sanitized.pop_back();
    }

    if (sanitized.empty()) {
        return "unknown";
    }

    return sanitized;
}

std::string build_dump_filename(std::string_view process_name, const SYSTEMTIME& local_time) {
    const std::string safe_name = sanitize_component(process_name);
    char timestamp[64] = {};
    std::snprintf(
        timestamp,
        std::size(timestamp),
        "%04u%02u%02u_%02u%02u%02u_%03u",
        local_time.wYear,
        local_time.wMonth,
        local_time.wDay,
        local_time.wHour,
        local_time.wMinute,
        local_time.wSecond,
        local_time.wMilliseconds);
    return safe_name + "_" + timestamp + ".dmp";
}

MINIDUMP_TYPE select_dump_type(bool full_memory) {
    const auto common_type = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithHandleData |
        MiniDumpWithThreadInfo |
        MiniDumpWithIndirectlyReferencedMemory);
    if (full_memory) {
        return static_cast<MINIDUMP_TYPE>(common_type | MiniDumpWithFullMemory);
    }
    return static_cast<MINIDUMP_TYPE>(MiniDumpNormal | common_type);
}

}  // namespace mhmodern::crash_handler::detail

namespace mhmodern::crash_handler {

bool install(const Settings& settings) {
    if (!settings.enabled) {
        return true;
    }

    const std::string safe_directory = detail::sanitize_component(settings.directory_name);
    g_dump_directory = exe_directory() / safe_directory;
    g_full_memory_dumps = settings.full_memory;

    std::error_code error_code;
    std::filesystem::create_directories(g_dump_directory, error_code);
    if (error_code) {
        logger::write(
            "Crash handler directory create failed: path=%s error=%s",
            g_dump_directory.string().c_str(),
            error_code.message().c_str());
        return false;
    }

    g_previous_filter = SetUnhandledExceptionFilter(&top_level_exception_filter);
    logger::write(
        "Crash handler installed: dir=%s fullMemory=%d",
        g_dump_directory.string().c_str(),
        settings.full_memory ? 1 : 0);
    return true;
}

}  // namespace mhmodern::crash_handler
