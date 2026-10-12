#pragma once

#include <string>

namespace sas {

auto progress_bar(int elapsed_seconds, int duration_seconds, int width = 20)
    -> std::string;

} // namespace sas
