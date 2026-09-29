#pragma once

#include "sas/model.hpp"

#include <string>
#include <optional>

namespace sas {

class Player {
 public:
  bool play(const Song& song, const std::string& stream_url, std::string& error);
  void pause();
  void resume();
  bool paused() const { return paused_; }
  void stop();
  const Song* current() const { return current_ ? &*current_ : nullptr; }

 private:
  std::string shell_escape(const std::string& value) const;
  std::optional<Song> current_;
  bool paused_ = false;
};

}  // namespace sas
