#include "sas/player.hpp"
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
namespace sas {
namespace {
auto executable_available(const char *name) -> bool {
  const char *path = std::getenv("PATH");
  if (!path)
    return false;
  std::stringstream paths(path);
  std::string directory;
  while (std::getline(paths, directory, ':')) {
    const auto candidate = std::filesystem::path(directory) / name;
    if (access(candidate.c_str(), X_OK) == 0)
      return true;
  }
  return false;
}
} // namespace

Player::~Player() { stop(); }
auto Player::shell_escape(const std::string &value) const -> std::string {
  std::string result = "'";
  for (const char c : value)
    result += c == '\'' ? "'\\''" : std::string(1, c);
  return result + "'";
}
auto Player::play(const Song &song, const std::string &stream_url,
                  std::string &error) -> bool {
  if (stream_url.empty()) {
    error = "Audio Station returned an empty stream URL";
    return false;
  }
  return play_playlist({song}, {stream_url}, error);
}
auto Player::play_playlist(const std::vector<Song> &songs,
                           const std::vector<std::string> &stream_urls,
                           std::string &error) -> bool {
  if (songs.empty() || songs.size() != stream_urls.size()) {
    error = "The playlist is empty";
    return false;
  }
  stop();
  const bool has_ffplay = executable_available("ffplay");
  const bool has_mpv = executable_available("mpv");
  const bool use_ffplay = preferred_player_ == "ffplay" ||
                          (preferred_player_ == "auto" && has_ffplay);
  if ((use_ffplay && !has_ffplay) || (!use_ffplay && !has_mpv)) {
    error = "Install ffmpeg (ffplay) or mpv to play audio";
    return false;
  }
  queue_stop_ = false;
  // Keep independent copies because the queue thread outlives this call.
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const auto queued_songs = songs;
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const auto urls = stream_urls;
  queue_thread_ =
      // All queue work is contained by the catch-all below.
      // NOLINTNEXTLINE(bugprone-exception-escape)
      std::thread([this, queued_songs, urls, use_ffplay]() noexcept -> void {
        try {
          do {
            for (size_t index = 0; index < urls.size(); ++index) {
              const auto &url = urls[index];
              if (queue_stop_ || url.empty())
                break;
              {
                std::scoped_lock lock(state_mutex_);
                current_ = queued_songs[index];
              }
              std::vector<std::string> arguments;
              if (use_ffplay) {
                arguments = {"ffplay", "-nodisp", "-autoexit", "-loglevel",
                             "warning"};
                if (insecure_tls_)
                  arguments.insert(arguments.end(), {"-tls_verify", "0"});
              } else {
                arguments = {"mpv", "--no-video", "--force-window=no"};
                if (insecure_tls_)
                  arguments.emplace_back("--tls-verify=no");
              }
              arguments.push_back(url);
              pid_t pid = -1;
              {
                // Synchronize cancellation with fork/PID publication so stop()
                // cannot finish before a newly created child becomes visible to
                // it.
                std::scoped_lock lock(process_mutex_);
                if (queue_stop_)
                  break;
                pid = fork();
                if (pid > 0)
                  child_pid_ = static_cast<int>(pid);
              }
              if (pid == 0) {
                const int null_fd = open("/dev/null", O_RDWR);
                if (null_fd >= 0) {
                  dup2(null_fd, STDIN_FILENO);
                  dup2(null_fd, STDOUT_FILENO);
                  dup2(null_fd, STDERR_FILENO);
                }
                std::vector<char *> argv;
                argv.reserve(arguments.size());
                for (auto &argument : arguments)
                  argv.push_back(argument.data());
                argv.push_back(nullptr);
                execvp(argv[0], argv.data());
                _exit(127);
              }
              if (pid < 0)
                break;
              int status = 0;
              waitpid(pid, &status, 0);
              {
                std::scoped_lock lock(process_mutex_);
                if (child_pid_ == pid)
                  child_pid_ = -1;
              }
            }
          } while (repeat_ && !queue_stop_);
          std::scoped_lock lock(state_mutex_);
          current_.reset();
        } catch (...) {
          std::scoped_lock lock(state_mutex_);
          current_.reset();
        }
      });
  {
    std::scoped_lock lock(state_mutex_);
    current_ = songs.front();
  }
  paused_ = false;
  return true;
}
void Player::pause() {
  if (!current() || paused_)
    return;
  std::scoped_lock lock(process_mutex_);
  const int pid = child_pid_.load();
  if (pid > 0)
    kill(static_cast<pid_t>(pid), SIGSTOP);
  paused_ = true;
}
void Player::resume() {
  if (!current() || !paused_)
    return;
  std::scoped_lock lock(process_mutex_);
  const int pid = child_pid_.load();
  if (pid > 0)
    kill(static_cast<pid_t>(pid), SIGCONT);
  paused_ = false;
}
void Player::stop() {
  queue_stop_ = true;
  {
    std::scoped_lock lock(process_mutex_);
    const int child_pid = child_pid_.load();
    if (child_pid > 0) {
      // A SIGSTOPped child will not process SIGTERM until it is continued.
      kill(static_cast<pid_t>(child_pid), SIGCONT);
      kill(static_cast<pid_t>(child_pid), SIGTERM);
    }
  }
  if (queue_thread_.joinable() &&
      queue_thread_.get_id() != std::this_thread::get_id())
    queue_thread_.join();
  {
    std::scoped_lock lock(state_mutex_);
    current_.reset();
  }
  paused_ = false;
}

auto Player::current() const -> std::optional<Song> {
  std::scoped_lock lock(state_mutex_);
  return current_;
}
} // namespace sas
