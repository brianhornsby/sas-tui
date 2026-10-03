#pragma once

#include "sas/model.hpp"

#include <string>
#include <optional>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>

namespace sas {

class Player {
 public:
  ~Player();
  bool play(const Song& song, const std::string& stream_url, std::string& error);
  bool play_playlist(const std::vector<Song>& songs, const std::vector<std::string>& stream_urls, std::string& error);
  void pause();
  void resume();
  bool paused() const { return paused_; }
  void stop();
  const Song* current() const { return current_ ? &*current_ : nullptr; }

 private:
  std::string shell_escape(const std::string& value) const;
  std::optional<Song> current_;
  bool paused_ = false;
  std::atomic<bool> queue_stop_{false};
  std::atomic<int> child_pid_{-1};
  std::mutex process_mutex_;
  std::thread queue_thread_;
};

}  // namespace sas
