#include "sas/synology_client.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <optional>

using json = nlohmann::json;

namespace sas {
namespace {
auto write_body(char *ptr, size_t size, size_t count, void *data) -> size_t {
  auto *out = static_cast<std::string *>(data);
  out->append(ptr, size * count);
  return size * count;
}

auto duration_seconds(const json &item) -> int {
  const auto read = [](const json &value) -> int {
    try {
      if (value.is_number_integer())
        return value.get<int>();
      if (value.is_number_float())
        return static_cast<int>(value.get<double>());
      if (value.is_string()) {
        const auto text = value.get<std::string>();
        const auto separator = text.find(':');
        if (separator != std::string::npos) {
          int seconds = 0;
          size_t start = 0;
          size_t next = separator;
          while (true) {
            seconds =
                seconds * 60 + std::stoi(text.substr(start, next - start));
            if (next == std::string::npos)
              break;
            start = next + 1;
            next = text.find(':', start);
          }
          return seconds;
        }
        return std::stoi(text);
      }
    } catch (const std::exception &) {
      return 0; // A missing or newer duration format is treated as unknown.
    }
    return 0;
  };

  if (item.contains("duration")) {
    const int value = read(item.at("duration"));
    if (value > 0)
      return value;
  }
  if (item.contains("additional") && item.at("additional").is_object() &&
      item.at("additional").contains("song_audio")) {
    const auto &audio = item.at("additional").at("song_audio");
    if (audio.is_object() && audio.contains("duration"))
      return read(audio.at("duration"));
  }
  return 0;
}
} // namespace

SynologyClient::SynologyClient(std::string base_url, bool insecure_tls)
    : base_url_(std::move(base_url)), insecure_tls_(insecure_tls) {
  while (!base_url_.empty() && base_url_.back() == '/')
    base_url_.pop_back();
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

SynologyClient::~SynologyClient() { curl_global_cleanup(); }

static auto escape(CURL *curl, const std::string &value) -> std::string {
  char *raw =
      curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
  std::string result = raw ? raw : value;
  curl_free(raw);
  return result;
}

static auto
request(const std::string &base,
        const std::string &path,
        const std::vector<std::pair<std::string, std::string>> &params,
        std::string &error,
        const std::string &token,
        bool insecure_tls) -> std::optional<std::string> {
  CURL *curl = curl_easy_init();
  if (!curl) {
    error = "Unable to initialize libcurl";
    return std::nullopt;
  }
  std::string url = base + path + "?";
  for (size_t i = 0; i < params.size(); ++i) {
    if (i)
      url += '&';
    url += escape(curl, params[i].first) + '=' + escape(curl, params[i].second);
  }
  if (!token.empty())
    url += "&SynoToken=" + escape(curl, token);
  std::string body;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "sas-tui/0.2");
  if (insecure_tls) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  const auto result = curl_easy_perform(curl);
  if (result != CURLE_OK)
    error = curl_easy_strerror(result);
  curl_easy_cleanup(curl);
  return result == CURLE_OK ? std::optional<std::string>(body) : std::nullopt;
}

auto SynologyClient::login(const std::string &account,
                           const std::string &password,
                           std::string &error) -> bool {
  std::string auth_path = "/webapi/auth.cgi";
  int auth_version = 3;
  if (auto info = request(base_url_, "/webapi/entry.cgi",
                          {{"api", "SYNO.API.Info"},
                           {"version", "1"},
                           {"method", "query"},
                           {"query", "SYNO.API.Auth"}},
                          error, "", insecure_tls_)) {
    try {
      const auto &auth = json::parse(*info).at("data").at("SYNO.API.Auth");
      auth_path = "/webapi/" + auth.value("path", "auth.cgi");
      auth_version = auth.value("maxVersion", 3);
    } catch (const std::exception &) {
      error.clear(); // Keep the default endpoint for older DSM versions.
    }
  }
  auto response = request(base_url_, auth_path,
                          {{"api", "SYNO.API.Auth"},
                           {"version", std::to_string(auth_version)},
                           {"method", "login"},
                           {"account", account},
                           {"passwd", password},
                           {"session", "AudioStation"},
                           {"format", "sid"},
                           {"enable_syno_token", "yes"}},
                          error, "", insecure_tls_);
  if (!response)
    return false;
  try {
    const auto data = json::parse(*response);
    if (!data.value("success", false)) {
      error = data.dump();
      return false;
    }
    sid_ = data.at("data").at("sid").get<std::string>();
    syno_token_ = data.at("data").value("synotoken", "");
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}

auto SynologyClient::artists(std::string &error) -> std::vector<std::string> {
  constexpr int page_size = 200;
  std::vector<std::string> result;
  for (int offset = 0; offset < 100000; offset += page_size) {
    auto response = request(base_url_, "/webapi/AudioStation/artist.cgi",
                            {{"api", "SYNO.AudioStation.Artist"},
                             {"version", "3"},
                             {"method", "list"},
                             {"library", "all"},
                             {"limit", std::to_string(page_size)},
                             {"offset", std::to_string(offset)},
                             {"_sid", sid_},
                             {"sort_by", "name"},
                             {"sort_direction", "ASC"}},
                            error, syno_token_, insecure_tls_);
    if (!response)
      return {};
    try {
      const auto data = json::parse(*response);
      if (!data.value("success", false)) {
        error = data.dump();
        return {};
      }
      const auto &items = data.at("data").at("artists");
      for (const auto &item : items) {
        auto value = item.value("name", item.value("display_artist", ""));
        if (!value.empty())
          result.push_back(value);
      }
      if (items.size() < page_size)
        break;
    } catch (const std::exception &e) {
      error = e.what();
      return {};
    }
  }
  return result;
}

auto SynologyClient::albums(const std::string &artist, std::string &error)
    -> std::vector<Album> {
  constexpr int page_size = 200;
  std::vector<Album> result;
  for (int offset = 0; offset < 100000; offset += page_size) {
    std::vector<std::pair<std::string, std::string>> params = {
        {"api", "SYNO.AudioStation.Album"},
        {"version", "3"},
        {"method", "list"},
        {"library", "all"},
        {"limit", std::to_string(page_size)},
        {"offset", std::to_string(offset)},
        {"_sid", sid_},
        {"sort_by", "name"},
        {"sort_direction", "ASC"}};
    if (!artist.empty())
      params.emplace_back("artist", artist);
    auto response = request(base_url_, "/webapi/AudioStation/album.cgi", params,
                            error, syno_token_, insecure_tls_);
    if (!response)
      return {};
    try {
      const auto data = json::parse(*response);
      if (!data.value("success", false)) {
        error = data.dump();
        return {};
      }
      const auto &items = data.at("data").at("albums");
      for (const auto &item : items)
        result.push_back(
            {.name = item.value("name", item.value("album", "")),
             .artist =
                 item.value("album_artist", item.value("display_artist", ""))});
      if (items.size() < page_size)
        break;
    } catch (const std::exception &e) {
      error = e.what();
      return {};
    }
  }
  return result;
}

auto SynologyClient::songs(const std::string &artist,
                           const std::string &album,
                           std::string &error) -> std::vector<Song> {
  constexpr int page_size = 200;
  std::vector<Song> result;
  std::string previous;
  for (int offset = 0; offset < 20000; offset += page_size) {
    std::vector<std::pair<std::string, std::string>> params = {
        {"api", "SYNO.AudioStation.Song"},
        {"version", "3"},
        {"method", "list"},
        {"library", "all"},
        {"limit", std::to_string(page_size)},
        {"offset", std::to_string(offset)},
        {"_sid", sid_},
        {"additional", "song_tag,song_audio"}};
    if (!artist.empty())
      params.emplace_back("artist", artist);
    if (!album.empty())
      params.emplace_back("album", album);
    auto response = request(base_url_, "/webapi/AudioStation/song.cgi", params,
                            error, syno_token_, insecure_tls_);
    if (!response)
      return {};
    try {
      const auto data = json::parse(*response);
      if (!data.value("success", false)) {
        error = data.dump();
        return {};
      }
      const auto &page = data.at("data").at("songs");
      if (page.empty())
        break;
      const auto first = page.front().value("id", "");
      if (!first.empty() && first == previous)
        break;
      previous = first;
      for (const auto &item : page) {
        Song song{.id = item.value("id", ""),
                  .title = item.value("title", "Untitled"),
                  .artist =
                      item.value("artist", item.value("display_artist", "")),
                  .album = item.value("album", ""),
                  .duration_seconds = duration_seconds(item)};
        if (item.contains("additional") &&
            item.at("additional").contains("song_tag")) {
          const auto &tag = item.at("additional").at("song_tag");
          if (song.artist.empty())
            song.artist = tag.value("artist", tag.value("artist_name", ""));
          if (song.album.empty())
            song.album = tag.value("album", tag.value("album_name", ""));
        }
        if (song.artist.empty())
          song.artist = "Unknown artist";
        if (song.album.empty())
          song.album = "Unknown album";
        result.push_back(std::move(song));
      }
      if (page.size() < page_size)
        break;
    } catch (const std::exception &e) {
      error = e.what();
      return {};
    }
  }
  return result;
}

auto SynologyClient::stream_url(const Song &song) const -> std::string {
  CURL *curl = curl_easy_init();
  if (!curl)
    return {};
  const auto id = escape(curl, song.id);
  const auto sid = escape(curl, sid_);
  const auto token = escape(curl, syno_token_);
  curl_easy_cleanup(curl);
  return base_url_ +
         "/webapi/AudioStation/"
         "stream.cgi?api=SYNO.AudioStation.Stream&version=2&method=stream&id=" +
         id + "&_sid=" + sid + "&SynoToken=" + token;
}

} // namespace sas
