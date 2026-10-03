#include "sas/player.hpp"
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
namespace sas {
Player::~Player() { stop(); }
std::string Player::shell_escape(const std::string& value) const { std::string result = "'"; for (const char c : value) result += c == '\'' ? "'\\''" : std::string(1, c); return result + "'"; }
bool Player::play(const Song& song, const std::string& stream_url, std::string& error) {
  if (stream_url.empty()) { error = "Audio Station returned an empty stream URL"; return false; }
  stop();

  std::string command;
  if (std::system("command -v ffplay >/dev/null 2>&1") == 0) {
    const char* insecure = std::getenv("SAS_TUI_INSECURE_TLS");
    const std::string tls = insecure && std::string(insecure) == "1" ? " -tls_verify 0" : "";
    command = "ffplay -nodisp -autoexit -loglevel warning" + tls + " " + shell_escape(stream_url) + " >/dev/null 2>&1 &";
  } else if (std::system("command -v mpv >/dev/null 2>&1") == 0) {
    command = "mpv --no-video --force-window=no " + shell_escape(stream_url) + " >/dev/null 2>&1 &";
  } else {
    error = "Install ffmpeg (ffplay) or mpv to play audio";
    return false;
  }
  if (std::system(command.c_str()) != 0) { error = "Unable to start audio player"; return false; }
  current_ = song; paused_ = false; return true;
}
bool Player::play_playlist(const std::vector<Song>& songs, const std::vector<std::string>& stream_urls, std::string& error) {
  if (songs.empty() || songs.size() != stream_urls.size()) { error = "The playlist is empty"; return false; }
  stop();
  const bool has_ffplay = std::system("command -v ffplay >/dev/null 2>&1") == 0;
  const bool has_mpv = std::system("command -v mpv >/dev/null 2>&1") == 0;
  if (!has_ffplay && !has_mpv) { error = "Install ffmpeg (ffplay) or mpv to play audio"; return false; }
  queue_stop_ = false;
  const auto urls = stream_urls;
  queue_thread_ = std::thread([this, urls, has_ffplay] {
    for (const auto& url : urls) {
      if (queue_stop_ || url.empty()) break;
      std::vector<std::string> arguments;
      if (has_ffplay) {
        arguments = {"ffplay", "-nodisp", "-autoexit", "-loglevel", "warning"};
        const char* insecure = std::getenv("SAS_TUI_INSECURE_TLS");
        if (insecure && std::string(insecure) == "1") arguments.insert(arguments.end(), {"-tls_verify", "0"});
      } else {
        arguments = {"mpv", "--no-video", "--force-window=no"};
      }
      arguments.push_back(url);
      pid_t pid = -1;
      {
        // Synchronize cancellation with fork/PID publication so stop() cannot
        // finish before a newly created child becomes visible to it.
        std::lock_guard<std::mutex> lock(process_mutex_);
        if (queue_stop_) break;
        pid = fork();
        if (pid > 0) child_pid_ = static_cast<int>(pid);
      }
      if (pid == 0) {
        const int null_fd = open("/dev/null", O_RDWR);
        if (null_fd >= 0) { dup2(null_fd, STDIN_FILENO); dup2(null_fd, STDOUT_FILENO); dup2(null_fd, STDERR_FILENO); }
        std::vector<char*> argv;
        for (auto& argument : arguments) argv.push_back(argument.data());
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
      }
      if (pid < 0) break;
      int status = 0;
      waitpid(pid, &status, 0);
      {
        std::lock_guard<std::mutex> lock(process_mutex_);
        if (child_pid_ == pid) child_pid_ = -1;
      }
    }
  });
  current_ = songs.front(); paused_ = false; return true;
}
void Player::pause() {
  if (!current_ || paused_) return;
  std::system("pkill -STOP -x ffplay >/dev/null 2>&1");
  std::system("pkill -STOP -x mpv >/dev/null 2>&1");
  paused_ = true;
}
void Player::resume() {
  if (!current_ || !paused_) return;
  std::system("pkill -CONT -x ffplay >/dev/null 2>&1");
  std::system("pkill -CONT -x mpv >/dev/null 2>&1");
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
  std::system("pkill -x ffplay >/dev/null 2>&1");
  std::system("pkill -x mpv >/dev/null 2>&1");
  if (queue_thread_.joinable() && queue_thread_.get_id() != std::this_thread::get_id()) queue_thread_.join();
  current_.reset(); paused_ = false;
}
}  // namespace sas
