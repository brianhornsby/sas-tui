#include "sas/config.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace sas {
namespace {

auto environment(const char *name) -> std::string {
  const char *value = std::getenv(name);
  return value ? value : "";
}

auto parse_bool(const json &value, bool &result) -> bool {
  if (value.is_boolean()) {
    result = value.get<bool>();
    return true;
  }
  if (value.is_number_integer()) {
    result = value.get<int>() != 0;
    return true;
  }
  return false;
}

auto valid_percent(int value) -> bool { return value >= 10 && value <= 80; }

auto load_file(const std::filesystem::path &path, Config &config,
               std::string &error) -> bool {
  std::ifstream input(path);
  if (!input) {
    error = "Cannot open config " + path.string();
    return false;
  }
  try {
    const auto document = json::parse(input);
    if (document.contains("url") && document.at("url").is_string())
      config.url = document.at("url").get<std::string>();
    if (document.contains("user") && document.at("user").is_string())
      config.user = document.at("user").get<std::string>();
    if (document.contains("player") && document.at("player").is_string())
      config.player = document.at("player").get<std::string>();
    if (document.contains("insecure_tls") &&
        !parse_bool(document.at("insecure_tls"), config.insecure_tls))
      error = "insecure_tls must be true or false";
    if (document.contains("ui") && document.at("ui").is_object()) {
      const auto &ui = document.at("ui");
      if (ui.contains("artist_width_percent"))
        config.ui.artist_width_percent =
            ui.at("artist_width_percent").get<int>();
      if (ui.contains("album_width_percent"))
        config.ui.album_width_percent = ui.at("album_width_percent").get<int>();
      if (ui.contains("now_playing_height_percent"))
        config.ui.now_playing_height_percent =
            ui.at("now_playing_height_percent").get<int>();
    }
  } catch (const std::exception &exception) {
    error = "Cannot read config " + path.string() + ": " + exception.what();
    return false;
  }
  return error.empty();
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
auto take_value(int argc, char **argv, int &index, const char *option,
                std::string &value, std::string &error) -> bool {
  const std::string argument = argv[index];
  if (argument == option) {
    if (index + 1 >= argc) {
      error = std::string(option) + " requires a value";
      return true;
    }
    value = argv[++index];
    return true;
  }
  const std::string prefix = std::string(option) + "=";
  if (argument.starts_with(prefix)) {
    value = argument.substr(prefix.size());
    return true;
  }
  return false;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

auto valid_player(const std::string &player) -> bool {
  return player == "auto" || player == "ffplay" || player == "mpv";
}

} // namespace

auto load_config(int argc, char **argv, Config &config, std::string &password,
                 std::string &error, bool &show_help) -> bool {
  show_help = false;
  std::string config_path = environment("SAS_TUI_CONFIG");
  bool explicit_config = !config_path.empty();
  for (int i = 1; i < argc; ++i) {
    std::string value;
    if (take_value(argc, argv, i, "--config", value, error)) {
      if (!error.empty())
        return false;
      config_path = value;
      explicit_config = true;
    } else if (std::string(argv[i]) == "--help" ||
               std::string(argv[i]) == "-h") {
      show_help = true;
      return true;
    }
  }
  if (config_path.empty()) {
    const std::string config_home = environment("XDG_CONFIG_HOME");
    if (!config_home.empty())
      config_path = config_home + "/sas-tui/config.json";
    else {
      const std::string home = environment("HOME");
      if (!home.empty())
        config_path = home + "/.config/sas-tui/config.json";
    }
  }
  if (!config_path.empty()) {
    const bool exists = std::filesystem::exists(config_path);
    if (exists && !load_file(config_path, config, error))
      return false;
    if (explicit_config && !exists) {
      error = "Config file not found: " + config_path;
      return false;
    }
  }

  if (const auto value = environment("SAS_TUI_URL"); !value.empty())
    config.url = value;
  if (const auto value = environment("SAS_TUI_USER"); !value.empty())
    config.user = value;
  if (const auto value = environment("SAS_TUI_PLAYER"); !value.empty())
    config.player = value;
  if (const auto value = environment("SAS_TUI_INSECURE_TLS"); !value.empty())
    config.insecure_tls = value == "1" || value == "true";
  password = environment("SAS_TUI_PASSWORD");

  for (int i = 1; i < argc; ++i) {
    std::string value;
    if (take_value(argc, argv, i, "--url", value, error))
      config.url = value;
    else if (take_value(argc, argv, i, "--user", value, error))
      config.user = value;
    else if (take_value(argc, argv, i, "--player", value, error))
      config.player = value;
    else if (std::string(argv[i]) == "--insecure-tls")
      config.insecure_tls = true;
    else if (std::string(argv[i]) == "--secure-tls")
      config.insecure_tls = false;
    else if (std::string(argv[i]) == "--config" ||
             std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h")
      ++i;
    else if (std::string(argv[i]).starts_with("--config=")) {
      // Already consumed during the config-file discovery pass.
    } else {
      error = "Unknown option: " + std::string(argv[i]);
      return false;
    }
    if (!error.empty())
      return false;
  }
  if (!valid_player(config.player)) {
    error = "player must be auto, ffplay, or mpv";
    return false;
  }
  if (!valid_percent(config.ui.artist_width_percent) ||
      !valid_percent(config.ui.album_width_percent) ||
      !valid_percent(config.ui.now_playing_height_percent)) {
    error = "UI percentages must be between 10 and 80";
    return false;
  }
  if (config.ui.artist_width_percent + config.ui.album_width_percent > 80) {
    error =
        "Artist and album widths must total 80% or less to keep Tracks visible";
    return false;
  }
  if (config.url.empty() || config.user.empty() || password.empty()) {
    error = "Set SAS_TUI_URL, SAS_TUI_USER, and SAS_TUI_PASSWORD (or use "
            "--url/--user and a password environment variable)";
    return false;
  }
  return true;
}

void print_help(const char *program) {
  std::cout
      << "Usage: " << program << " [options]\n\n"
      << "Options:\n"
      << "  -h, --help              Show this help\n"
      << "      --config PATH       Use a JSON config file\n"
      << "      --url URL           Override the Audio Station URL\n"
      << "      --user USER         Override the Audio Station user\n"
      << "      --player PLAYER     auto, ffplay, or mpv\n"
      << "      --insecure-tls      Disable TLS certificate verification\n"
      << "      --secure-tls        Require TLS certificate verification\n\n"
      << "SAS_TUI_PASSWORD is read from the environment and is never accepted "
         "on the command line or in config files.\n";
}

} // namespace sas
