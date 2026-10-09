#include "sas/player.hpp"
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
namespace sas {
Player::~Player() { stop(); }
std::string Player::shell_escape(const std::string &value) const {
  std::string result = "'";
  for (const char c : value)
    result += c == '\'' ? "'\\''" : std::string(1, c);
  return result + "'";
}
bool Player::play(const Song &song, const std::string &stream_url,
                  std::string &error) {
  if (stream_url.empty()) {
    error = "Audio Station returned an empty stream URL";
    return false;
  }
  return play_playlist({song}, {stream_url}, error);
}
bool Player::play_playlist(const std::vector<Song> &songs,
                           const std::vector<std::string> &stream_urls,
                           std::string &error) {
  if (songs.empty() || songs.size() != stream_urls.size()) {
    error = "The playlist is empty";
    return false;
  }
  stop();
  const bool has_ffplay = std::system("command -v ffplay >/dev/null 2>&1") == 0;
  const bool has_mpv = std::system("command -v mpv >/dev/null 2>&1") == 0;
  if (!has_ffplay && !has_mpv) {
    error = "Install ffmpeg (ffplay) or mpv to play audio";
    return false;
  }
  queue_stop_ = false;
  const auto queued_songs = songs;
  const auto urls = stream_urls;
  queue_thread_ = std::thread([this, queued_songs, urls, has_ffplay] {
    do {
      for (size_t index = 0; index < urls.size(); ++index) {
        const auto &url = urls[index];
        if (queue_stop_ || url.empty())
          break;
        {
          std::lock_guard<std::mutex> lock(state_mutex_);
          current_ = queued_songs[index];
        }
        std::vector<std::string> arguments;
        if (has_ffplay) {
          arguments = {"ffplay", "-nodisp", "-autoexit", "-loglevel",
                       "warning"};
          const char *insecure = std::getenv("SAS_TUI_INSECURE_TLS");
          if (insecure && std::string(insecure) == "1")
            arguments.insert(arguments.end(), {"-tls_verify", "0"});
        } else {
          arguments = {"mpv", "--no-video", "--force-window=no"};
        }
        arguments.push_back(url);
        pid_t pid = -1;
        {
          // Synchronize cancellation with fork/PID publication so stop() cannot
          // finish before a newly created child becomes visible to it.
          std::lock_guard<std::mutex> lock(process_mutex_);
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
          std::lock_guard<std::mutex> lock(process_mutex_);
          if (child_pid_ == pid)
            child_pid_ = -1;
        }
      }
    } while (repeat_ && !queue_stop_);
    std::lock_guard<std::mutex> lock(state_mutex_);
    current_.reset();
  });
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    current_ = songs.front();
  }
  paused_ = false;
  return true;
}
void Player::pause() {
  if (!current() || paused_)
    return;
  std::lock_guard<std::mutex> lock(process_mutex_);
  const int pid = child_pid_.load();
  if (pid > 0)
    kill(static_cast<pid_t>(pid), SIGSTOP);
  paused_ = true;
}
void Player::resume() {
  if (!current() || !paused_)
    return;
  std::lock_guard<std::mutex> lock(process_mutex_);
  const int pid = child_pid_.load();
  if (pid > 0)
    kill(static_cast<pid_t>(pid), SIGCONT);
  paused_ = false;
}
void Player::stop() {
  queue_stop_ = true;
  {
    std::lock_guard<std::mutex> lock(process_mutex_);
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
    std::lock_guard<std::mutex> lock(state_mutex_);
    current_.reset();
  }
  paused_ = false;
}

std::optional<Song> Player::current() const {
  std::lock_guard<std::mutex> lock(state_mutex_);
  return current_;
}
} // namespace sas
