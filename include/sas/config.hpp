#pragma once

#include <string>

namespace sas {

struct UiConfig {
  int artist_width_percent = 25;
  int album_width_percent = 25;
  int now_playing_height_percent = 20;
};

struct Config {
  std::string url;
  std::string user;
  std::string player = "auto";
  bool insecure_tls = false;
  UiConfig ui;
};

// Load defaults, the optional JSON config file, environment overrides, and
// command-line overrides in that order. Passwords intentionally remain
// environment-only and are not represented by this structure.
auto load_config(int argc, char **argv, Config &config, std::string &password,
                 std::string &error, bool &show_help) -> bool;

void print_help(const char *program);

} // namespace sas
