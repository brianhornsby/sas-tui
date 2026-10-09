#include "sas/config.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void set_environment(const char *name, const std::string &value) {
  setenv(name, value.c_str(), 1);
}

void clear_environment() {
  unsetenv("SAS_TUI_CONFIG");
  unsetenv("SAS_TUI_URL");
  unsetenv("SAS_TUI_USER");
  unsetenv("SAS_TUI_PASSWORD");
  unsetenv("SAS_TUI_PLAYER");
  unsetenv("SAS_TUI_INSECURE_TLS");
}

auto load(int argc,
          char **argv,
          sas::Config &config,
          std::string &password,
          std::string &error,
          bool &show_help) -> bool {
  return sas::load_config(argc, argv, config, password, error, show_help);
}

void test_environment_and_cli_precedence() {
  clear_environment();
  set_environment("SAS_TUI_URL", "https://nas.example.test:5001");
  set_environment("SAS_TUI_USER", "music");
  set_environment("SAS_TUI_PASSWORD", "secret");
  set_environment("SAS_TUI_PLAYER", "ffplay");

  char arg0[] = "sas-tui";
  char arg1[] = "--player";
  char arg2[] = "mpv";
  char *argv[] = {arg0, arg1, arg2};
  sas::Config config;
  std::string password;
  std::string error;
  bool show_help = false;

  assert(load(3, argv, config, password, error, show_help));
  assert(config.url == "https://nas.example.test:5001");
  assert(config.user == "music");
  assert(config.player == "mpv");
  assert(password == "secret");
  assert(!show_help);
}

void test_config_file_and_insecure_tls() {
  clear_environment();
  set_environment("SAS_TUI_PASSWORD", "secret");
  const auto path =
      std::filesystem::temp_directory_path() / "sas-tui-config-test.json";
  {
    std::ofstream output(path);
    output << R"({
      "url": "https://nas.example.test:5001",
      "user": "music",
      "player": "mpv",
      "insecure_tls": true,
      "ui": {
        "artist_width_percent": 20,
        "album_width_percent": 30
      }
    })";
  }

  std::string config_argument = "--config=" + path.string();
  char arg0[] = "sas-tui";
  char *argv[] = {arg0, config_argument.data()};
  sas::Config config;
  std::string password;
  std::string error;
  bool show_help = false;

  assert(load(2, argv, config, password, error, show_help));
  assert(config.insecure_tls);
  assert(config.ui.artist_width_percent == 20);
  assert(config.ui.album_width_percent == 30);
  std::filesystem::remove(path);
}

void test_invalid_widths_are_rejected() {
  clear_environment();
  set_environment("SAS_TUI_URL", "https://nas.example.test:5001");
  set_environment("SAS_TUI_USER", "music");
  set_environment("SAS_TUI_PASSWORD", "secret");
  const auto path = std::filesystem::temp_directory_path() /
                    "sas-tui-invalid-config-test.json";
  {
    std::ofstream output(path);
    output << R"({
      "ui": {
        "artist_width_percent": 50,
        "album_width_percent": 40
      }
    })";
  }
  std::string config_argument = "--config=" + path.string();
  char arg0[] = "sas-tui";
  char *argv[] = {arg0, config_argument.data()};
  sas::Config config;
  std::string password;
  std::string error;
  bool show_help = false;

  assert(!load(2, argv, config, password, error, show_help));
  assert(error.find("Tracks visible") != std::string::npos);
  std::filesystem::remove(path);
}

} // namespace

int main() {
  test_environment_and_cli_precedence();
  test_config_file_and_insecure_tls();
  test_invalid_widths_are_rejected();
  clear_environment();
  return 0;
}
