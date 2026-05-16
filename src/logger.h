#pragma once

#include <string>

namespace mhmodern::logger {

bool initialize();
void write(const char* format, ...);
std::string log_path();

}  // namespace mhmodern::logger
