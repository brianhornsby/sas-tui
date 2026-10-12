#pragma once

#include <string>

namespace sas {

// Keep this declaration stable across clang-format versions.
// clang-format off
auto progress_bar(int elapsed_seconds, int duration_seconds, int width = 20)
    -> std::string;
// clang-format on

} // namespace sas
