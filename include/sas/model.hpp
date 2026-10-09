#pragma once

#include <string>

namespace sas {

struct Song {
  std::string id;
  std::string title;
  std::string artist;
  std::string album;
  int duration_seconds = 0;
};

struct Album {
  std::string name;
  std::string artist;
};

} // namespace sas
