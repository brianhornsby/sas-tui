#pragma once

#include "sas/model.hpp"

#include <string>
#include <vector>

namespace sas {

class SynologyClient {
public:
  explicit SynologyClient(std::string base_url);
  ~SynologyClient();

  bool login(const std::string &account, const std::string &password,
             std::string &error);
  std::vector<std::string> artists(std::string &error);
  std::vector<Album> albums(const std::string &artist, std::string &error);
  std::vector<Song> songs(const std::string &artist, const std::string &album,
                          std::string &error);
  std::string stream_url(const Song &song) const;

private:
  std::string base_url_;
  std::string sid_;
  std::string syno_token_;
  std::string last_error_;
};

} // namespace sas
