#include "sas/player.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unistd.h>
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
bool Player::play_playlist(const std::vector<Song>& songs, const std::vector<std::string>& stream_urls, std::string& error) {
  if (songs.empty() || songs.size() != stream_urls.size()) { error = "The playlist is empty"; return false; }
  stop();
  playlist_path_ = "/tmp/sas-tui-playlist-" + std::to_string(static_cast<long long>(getpid())) + ".txt";
  std::ofstream playlist(playlist_path_);
  if (!playlist) { error = "Unable to create the playback playlist"; return false; }
  for (const auto& url : stream_urls) playlist << "file " << shell_escape(url) << "\n";
  playlist.close();
  if (std::system("command -v ffplay >/dev/null 2>&1") != 0) {
    error = "Playlist playback requires ffplay (install ffmpeg)";
    std::remove(playlist_path_.c_str());
    playlist_path_.clear();
    return false;
  }
  const char* insecure = std::getenv("SAS_TUI_INSECURE_TLS");
  const std::string tls = insecure && std::string(insecure) == "1" ? " -tls_verify 0" : "";
  const std::string command = "ffplay -nodisp -autoexit -loglevel warning -f concat -safe 0 -protocol_whitelist file,http,https,tcp,tls,crypto" + tls + " -i " + shell_escape(playlist_path_) + " >/dev/null 2>&1 &";
  if (std::system(command.c_str()) != 0) { error = "Unable to start playlist playback"; return false; }
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
  std::system("pkill -x ffplay >/dev/null 2>&1");
  std::system("pkill -x mpv >/dev/null 2>&1");
  current_.reset(); paused_ = false;
  if (!playlist_path_.empty()) { std::remove(playlist_path_.c_str()); playlist_path_.clear(); }
}
}  // namespace sas
