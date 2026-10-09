#pragma once

#include "sas/model.hpp"

#include <string>
#include <vector>

namespace sas {

class SynologyClient {
public:
  explicit SynologyClient(std::string base_url, bool insecure_tls = false);
  ~SynologyClient();

  auto login(const std::string &account,
             const std::string &password,
             std::string &error) -> bool;
  auto artists(std::string &error) -> std::vector<std::string>;
  auto albums(const std::string &artist, std::string &error)
      -> std::vector<Album>;
  auto songs(const std::string &artist,
             const std::string &album,
             std::string &error) -> std::vector<Song>;
  [[nodiscard]] auto stream_url(const Song &song) const -> std::string;

private:
  std::string base_url_;
  std::string sid_;
  std::string syno_token_;
  std::string last_error_;
  bool insecure_tls_ = false;
};

} // namespace sas
