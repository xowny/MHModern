#pragma once

#include <windows.h>
#include <DbgHelp.h>

#include <string>
#include <string_view>

namespace mhmodern::crash_handler {

struct Settings {
    bool enabled = true;
    bool full_memory = false;
    std::string directory_name = "MHModern_crashes";
};

bool install(const Settings& settings);

namespace detail {
std::string sanitize_component(std::string_view value);
std::string build_dump_filename(std::string_view process_name, const SYSTEMTIME& local_time);
MINIDUMP_TYPE select_dump_type(bool full_memory);
}

}  // namespace mhmodern::crash_handler
