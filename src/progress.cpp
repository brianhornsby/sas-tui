#include "sas/progress.hpp"

#include <algorithm>

namespace sas {

auto progress_bar(int elapsed_seconds, int duration_seconds, int width)
    -> std::string {
  const int safe_width = std::max(1, width);
  std::string progress(static_cast<size_t>(safe_width), ' ');
  if (duration_seconds <= 0)
    return progress;

  const auto scaled_elapsed =
      static_cast<long long>(elapsed_seconds) * safe_width;
  const int filled =
      static_cast<int>(std::clamp(scaled_elapsed / duration_seconds, 0LL,
                                  static_cast<long long>(safe_width)));
  if (filled > 0) {
    progress.replace(0, static_cast<size_t>(filled - 1),
                     static_cast<size_t>(filled - 1), '=');
    progress[static_cast<size_t>(filled - 1)] = '>';
  }
  return progress;
}

} // namespace sas
