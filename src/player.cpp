#include "sas/player.hpp"
#include <cstdlib>
namespace sas {
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
  std::system("pkill -x ffplay >/dev/null 2>&1");
  std::system("pkill -x mpv >/dev/null 2>&1");
  current_.reset(); paused_ = false;
}
}  // namespace sas
