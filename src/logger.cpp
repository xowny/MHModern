#include "logger.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace {

std::mutex g_log_mutex;
std::filesystem::path g_log_path;
bool g_initialized = false;

std::filesystem::path exe_directory() {
    char buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) {
        return std::filesystem::current_path();
    }

    return std::filesystem::path(buffer).parent_path();
}

std::string timestamp() {
    SYSTEMTIME now{};
    GetLocalTime(&now);

    char buffer[64] = {};
    std::snprintf(buffer,
                  std::size(buffer),
                  "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                  now.wYear,
                  now.wMonth,
                  now.wDay,
                  now.wHour,
                  now.wMinute,
                  now.wSecond,
                  now.wMilliseconds);
    return buffer;
}

}  // namespace

namespace mhmodern::logger {

bool initialize() {
    std::scoped_lock lock(g_log_mutex);
    if (g_initialized) {
        return true;
    }

    g_log_path = exe_directory() / "MHModern.log";
    std::ofstream stream(g_log_path, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }

    stream << "[" << timestamp() << "] MHModern logger initialized\n";
    g_initialized = true;
    return true;
}

void write(const char* format, ...) {
    std::scoped_lock lock(g_log_mutex);
    if (!g_initialized && !initialize()) {
        return;
    }

    char message[1024] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, std::size(message), format, args);
    va_end(args);

    std::ofstream stream(g_log_path, std::ios::out | std::ios::app);
    if (!stream.is_open()) {
        return;
    }

    stream << "[" << timestamp() << "] " << message << '\n';
}

std::string log_path() {
    std::scoped_lock lock(g_log_mutex);
    if (!g_initialized && !initialize()) {
        return {};
    }
    return g_log_path.string();
}

}  // namespace mhmodern::logger
