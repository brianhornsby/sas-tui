#include "sas/app.hpp"
#include "sas/player.hpp"
#include "sas/synology_client.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>

using namespace ftxui;

namespace sas {

int run() {
  const char *url = std::getenv("SAS_TUI_URL");
  const char *user = std::getenv("SAS_TUI_USER");
  const char *password = std::getenv("SAS_TUI_PASSWORD");
  if (!url || !user || !password) {
    std::cerr << "Set SAS_TUI_URL, SAS_TUI_USER, and SAS_TUI_PASSWORD.\n";
    return 1;
  }

  auto client = std::make_shared<SynologyClient>(url);
  Player player;
  auto screen = ScreenInteractive::Fullscreen();
  std::string status = "Loading library...";
  bool loading = true;
  std::vector<Song> songs;
  std::vector<Album> album_catalog;
  std::vector<std::string> artists{"All Artists"}, albums{"All Albums"},
      track_labels;
  int artist_index = 0, album_index = 0, track_index = 0;
  std::string now = "Nothing playing";
  std::optional<Song> now_song;
  std::string playback_state = "Stopped";
  std::chrono::steady_clock::time_point playback_started;
  int elapsed_seconds = 0;
  std::string last_artist;
  std::string last_album;
  std::string album_catalog_artist;
  bool selection_loading = false;
  std::vector<std::thread> workers;
  std::unordered_map<std::string, std::vector<Song>> song_cache;
  std::unordered_map<std::string, std::vector<Album>> album_cache;

  auto refresh_albums = [&] {
    const auto artist = artist_index ? artists[artist_index] : "";
    const auto selected_album = album_index ? albums[album_index] : "";
    std::vector<std::string> filtered{"All Albums"};
    auto add_album = [&](const std::string &album) {
      if (!album.empty() && std::find(filtered.begin() + 1, filtered.end(),
                                      album) == filtered.end())
        filtered.push_back(album);
    };

    // Use song metadata as well as the album endpoint's artist field. This
    // handles compilations and albums with multiple contributing artists.
    for (const auto &album : album_catalog) {
      // When the catalog was requested for this artist, Audio Station has
      // already applied the filter. Its album_artist metadata can be blank or
      // differ for compilations, so do not filter those results a second time.
      const bool catalog_is_scoped =
          !artist.empty() && album_catalog_artist == artist;
      if (artist.empty() || catalog_is_scoped || album.artist == artist)
        add_album(album.name);
    }
    for (const auto &song : songs)
      if (artist.empty() || song.artist == artist)
        add_album(song.album);
    std::sort(filtered.begin() + 1, filtered.end());

    if (filtered != albums) {
      albums = std::move(filtered);
      album_index = 0;
      if (!selected_album.empty()) {
        const auto match =
            std::find(albums.begin() + 1, albums.end(), selected_album);
        if (match != albums.end())
          album_index = static_cast<int>(match - albums.begin());
      }
    }
  };

  auto rebuild = [&] {
    refresh_albums();
    track_labels.clear();
    const auto artist = artist_index ? artists[artist_index] : "";
    const auto album = album_index ? albums[album_index] : "";
    for (const auto &song : songs)
      if ((artist.empty() || song.artist == artist) &&
          (album.empty() || song.album == album))
        track_labels.push_back(song.title + "  —  " + song.artist);
    track_index = std::min(
        track_index, std::max(0, static_cast<int>(track_labels.size()) - 1));
  };

  int focus_index = 0;
  int artist_scroll = 0, album_scroll = 0, track_scroll = 0;
  auto marquee = [](const std::string& value, int& offset, int width) {
    if (width < 1 || static_cast<int>(value.size()) <= width) return value;
    const std::string loop = value + "   ";
    offset %= static_cast<int>(loop.size());
    return loop.substr(static_cast<size_t>(offset), static_cast<size_t>(width));
  };
  auto artist_option = MenuOption::Vertical();
  artist_option.entries = &artists;
  artist_option.selected = &artist_index;
  artist_option.entries_option.transform = [&](const EntryState& state) {
    auto label = (state.active ? "> " : "  ") + state.label;
    if (state.active) label = (state.active ? "> " : "  ") + marquee(state.label, artist_scroll, std::max(8, Terminal::Size().dimx * 20 / 100 - 6));
    auto element = text(label);
    if (state.active) element = element | bold;
    if (state.focused) element = element | inverted;
    return element;
  };
  auto album_option = MenuOption::Vertical();
  album_option.entries = &albums;
  album_option.selected = &album_index;
  album_option.entries_option.transform = [&](const EntryState& state) {
    auto label = (state.active ? "> " : "  ") + state.label;
    if (state.active) label = (state.active ? "> " : "  ") + marquee(state.label, album_scroll, std::max(8, Terminal::Size().dimx * 25 / 100 - 6));
    auto element = text(label);
    if (state.active) element = element | bold;
    if (state.focused) element = element | inverted;
    return element;
  };
  auto track_option = MenuOption::Vertical();
  track_option.entries = &track_labels;
  track_option.selected = &track_index;
  track_option.entries_option.transform = [&](const EntryState& state) {
    auto label = (state.active ? "> " : "  ") + state.label;
    if (state.active) {
      const int width = std::max(8, Terminal::Size().dimx - Terminal::Size().dimx * 20 / 100 - Terminal::Size().dimx * 25 / 100 - 8);
      label = (state.active ? "> " : "  ") + marquee(state.label, track_scroll, width);
    }
    auto element = text(label);
    if (state.active) element = element | bold;
    if (state.focused) element = element | inverted;
    return element;
  };
  auto artist_menu = Menu(artist_option);
  auto album_menu = Menu(album_option);
  auto track_menu = Menu(track_option);
  auto play_selected = [&](bool queue_all = false) {
    if (loading) return;
    const auto artist = artist_index ? artists[artist_index] : "";
    const auto album = album_index ? albums[album_index] : "";
    std::vector<Song> filtered;
    for (const auto &song : songs)
      if ((artist.empty() || song.artist == artist) &&
          (album.empty() || song.album == album))
        filtered.push_back(song);
    if (track_index < 0 || track_index >= static_cast<int>(filtered.size())) return;
    std::string error;
    bool started = false;
    if (queue_all) {
      std::vector<std::string> stream_urls;
      stream_urls.reserve(filtered.size());
      for (const auto& song : filtered) stream_urls.push_back(client->stream_url(song));
      started = player.play_playlist(filtered, stream_urls, error);
    } else {
      started = player.play(filtered[track_index], client->stream_url(filtered[track_index]), error);
    }
    if (started) {
      now_song = filtered[track_index];
      now = now_song->title + " — " + now_song->artist;
      playback_state = "Playing";
      elapsed_seconds = 0;
      playback_started = std::chrono::steady_clock::now();
    } else {
      status = error;
    }
  };
  auto play = CatchEvent(track_menu, [&](Event event) {
    if (event != Event::Return || loading)
      return false;
    play_selected();
    return true;
  });

  int selection_generation = 0;
  auto load_selection = [&](std::string artist, std::string album,
                            bool load_albums) {
    const int generation = ++selection_generation;
    selection_loading = true;
    songs.clear();
    track_labels.clear();
    status = load_albums ? "Loading albums and tracks..." : "Loading tracks...";
    workers.emplace_back([&, artist = std::move(artist),
                          album = std::move(album), load_albums, generation] {
      std::string album_error, song_error;
      std::vector<Album> loaded_albums;
      if (load_albums)
        loaded_albums = client->albums(artist, album_error);
      auto loaded_songs = client->songs(artist, album, song_error);
      screen.Post([&, loaded_albums = std::move(loaded_albums),
                   loaded_songs = std::move(loaded_songs), artist, album,
                   load_albums, generation, album_error, song_error]() mutable {
        if (generation != selection_generation)
          return;
        if (load_albums) {
          album_catalog = std::move(loaded_albums);
          album_catalog_artist = artist;
          if (album_error.empty())
            album_cache[artist] = album_catalog;
          albums = {"All Albums"};
          for (const auto &value : album_catalog) {
            if (std::find(albums.begin() + 1, albums.end(), value.name) ==
                albums.end())
              albums.push_back(value.name);
          }
          std::sort(albums.begin() + 1, albums.end());
          album_index = 0;
        }
        songs = std::move(loaded_songs);
        if (song_error.empty())
          song_cache[artist] = songs;
        selection_loading = false;
        if (!song_error.empty())
          status = "Tracks: " + song_error;
        else if (!album_error.empty())
          status = "Albums: " + album_error;
        else
          status = "Loaded " + std::to_string(songs.size()) + " tracks";
        rebuild();
      });
    });
  };

  auto sync_selection = [&] {
    if (loading || selection_loading || artists.empty())
      return;
    const auto artist = artist_index ? artists[artist_index] : "";
    if (artist != last_artist) {
      last_artist = artist;
      last_album.clear();
      album_index = 0;
      const auto cached_albums = album_cache.find(artist);
      const auto cached_songs = song_cache.find(artist);
      if (cached_albums != album_cache.end() &&
          cached_songs != song_cache.end()) {
        album_catalog = cached_albums->second;
        album_catalog_artist = artist;
        songs = cached_songs->second;
        albums = {"All Albums"};
        for (const auto &value : album_catalog)
          if (std::find(albums.begin() + 1, albums.end(), value.name) ==
              albums.end())
            albums.push_back(value.name);
        std::sort(albums.begin() + 1, albums.end());
        selection_loading = false;
        status = "Loaded " + std::to_string(songs.size()) + " tracks (cached)";
        rebuild();
        return;
      }
      albums = {"All Albums"};
      album_catalog.clear();
      album_catalog_artist.clear();
      load_selection(artist, "", true);
      return;
    }
    const auto album = album_index ? albums[album_index] : "";
    if (album != last_album) {
      last_album = album;
      // The artist request loads the complete artist track list. Album
      // selection is therefore an in-memory filter and needs no request.
      rebuild();
    }
  };

  auto tabs = Container::Tab({artist_menu, album_menu, play}, &focus_index);
  auto root = CatchEvent(tabs, [&](Event event) {
    if (event == Event::Custom) {
      if (now_song && playback_state == "Playing") {
        elapsed_seconds = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - playback_started).count());
        if (now_song->duration_seconds > 0) elapsed_seconds = std::min(elapsed_seconds, now_song->duration_seconds);
      }
      if (focus_index == 0 && !artists.empty() && artist_index < static_cast<int>(artists.size())) ++artist_scroll;
      if (focus_index == 1 && !albums.empty() && album_index < static_cast<int>(albums.size())) ++album_scroll;
      if (focus_index == 2 && !track_labels.empty() && track_index < static_cast<int>(track_labels.size())) ++track_scroll;
      screen.RequestAnimationFrame();
      return true;
    }
    // Menu consumes Tab to advance its own selection, so handle focus
    // navigation before the event reaches the active menu.
    if (event == Event::Tab || event == Event::TabReverse || event == Event::ArrowLeft || event == Event::ArrowRight) {
      const bool forward = event == Event::Tab || event == Event::ArrowRight;
      focus_index = (focus_index + (forward ? 1 : 2)) % 3;
      tabs->SetActiveChild(focus_index == 0   ? artist_menu
                           : focus_index == 1 ? album_menu
                                              : play);
      tabs->TakeFocus();
      return true;
    }
    if (event == Event::Character('q') || event == Event::Escape) {
      screen.Exit();
      return true;
    }
    if (event == Event::Return && focus_index != 2) {
      if (!track_labels.empty()) {
        track_index = 0;
        play_selected(true);
      }
      return true;
    }
    if (event == Event::Character('s')) {
      player.stop();
      now = "Stopped";
      playback_state = "Stopped";
      now_song.reset();
      return true;
    }
    if (event == Event::Character('[') || event == Event::Character(']')) {
      if (!track_labels.empty()) {
        const int direction = event == Event::Character(']') ? 1 : -1;
        track_index = (track_index + direction + static_cast<int>(track_labels.size())) % static_cast<int>(track_labels.size());
        play_selected();
      }
      return true;
    }
    if (event == Event::Character(' ')) {
      if (playback_state == "Playing") { player.pause(); playback_state = "Paused"; }
      else if (playback_state == "Paused") { player.resume(); playback_state = "Playing"; playback_started = std::chrono::steady_clock::now() - std::chrono::seconds(elapsed_seconds); }
      return true;
    }
    if (!loading) {
      sync_selection();
      rebuild();
    }
    return false;
  });

  const auto bg = Color::RGB(26, 27, 38), panel = Color::RGB(36, 40, 59),
             blue = Color::RGB(122, 162, 247),
             purple = Color::RGB(187, 154, 247),
             cyan = Color::RGB(125, 207, 255), muted = Color::RGB(86, 95, 137),
             fg = Color::RGB(192, 202, 245);
  auto view = Renderer(root, [&] {
    if (!loading)
      rebuild();
    const int artist_width = std::max(22, Terminal::Size().dimx * 25 / 100);
    const int album_width = std::max(28, Terminal::Size().dimx * 25 / 100);
    const int now_height = std::max(8, (Terminal::Size().dimy - 4) * 20 / 100);
    // Keep Albums visible even when the artist list is long. The artist pane
    // uses the remaining space after this minimum allocation.
    auto artist_panel =
        vbox({text(" ARTISTS") | bold | color(purple), separator(),
              artist_menu->Render() | xframe | yframe | flex}) |
        bgcolor(panel) | borderStyled(focus_index == 0 ? cyan : muted) | flex;
    auto album_list =
        album_menu->Render() | xframe | yframe | flex | size(HEIGHT, GREATER_THAN, 10);
    auto album_panel = vbox({text(" ALBUMS") | bold | color(purple),
                             separator(), album_list}) |
                       bgcolor(panel) |
                       borderStyled(focus_index == 1 ? cyan : muted);
    auto playlist_body = track_labels.empty()
                             ? filler()
                             : vscroll_indicator(play->Render() | xframe | yframe | flex);
    auto playlist = vbox({text(" TRACKS") | bold | color(purple), separator(),
                          playlist_body | flex}) |
                    bgcolor(panel) |
                    borderStyled(focus_index == 2 ? cyan : muted) | flex;
    const auto format_time = [](int seconds) {
      return std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
    };
    std::string progress(20, ' ');
    std::string timing = format_time(elapsed_seconds) + " / --:--";
    if (now_song && now_song->duration_seconds > 0) {
      const int filled = std::min(20, elapsed_seconds * 20 / std::max(1, now_song->duration_seconds));
      progress = std::string(static_cast<size_t>(filled), '=') + ">" + std::string(static_cast<size_t>(19 - filled), ' ');
      timing = format_time(elapsed_seconds) + " / " + format_time(now_song->duration_seconds);
    }
    const std::string now_title = now_song ? now_song->title : "Nothing playing";
    const std::string now_artist = now_song ? now_song->artist : "";
    const std::string now_album = now_song ? now_song->album : "";
    const std::string queue_position = track_labels.empty() ? "Queue empty" : "Track " + std::to_string(track_index + 1) + " of " + std::to_string(track_labels.size());
    auto now_view =
        vbox({text(" NOW PLAYING") | bold | color(purple), separator(),
              text("♫") | color(cyan) | center, text(now_title) | color(fg) | center,
              text(now_artist + (now_album.empty() ? "" : "  —  " + now_album)) | color(muted) | center,
              text(playback_state) | color(cyan) | center,
              text(queue_position) | color(muted) | center,
              text("[" + progress + "]") | color(blue) | center,
              text(timing) | color(muted) | center,
              text("[ previous   ] next   Space pause/resume   s stop") | color(muted) | center}) |
        bgcolor(panel) | borderStyled(muted) |
        size(HEIGHT, GREATER_THAN, now_height);
    auto tracks_column = vbox({playlist, now_view}) | flex;
    auto top = hbox({artist_panel | size(WIDTH, EQUAL, artist_width),
                     album_panel | size(WIDTH, EQUAL, album_width), tracks_column}) |
               flex;
    auto footer =
        hbox({text(" " + status + "   ") | color(muted),
              text("Tab/Arrows navigate   Enter play   s stop   q quit") |
                  color(muted)}) |
        bgcolor(panel);
    return vbox({text(" sas-tui  //  Synology Audio Station") | bold |
                     color(cyan) | bgcolor(panel),
                 top, footer}) |
           bgcolor(bg) | color(fg);
  });

  const std::string account = user;
  const std::string secret = password;
  std::thread loader([&, account, secret] {
    std::string error;
    screen.Post([&] { status = "Loading artists..."; });
    if (!client->login(account, secret, error)) {
      screen.Post([&, error] {
        status = "Login failed: " + error;
        loading = false;
      });
      return;
    }
    std::string artist_error;
    auto loaded_artists = client->artists(artist_error);
    screen.Post([&, loaded_artists = std::move(loaded_artists), artist_error] {
      artists = {"All Artists"};
      artists.insert(artists.end(), loaded_artists.begin(),
                     loaded_artists.end());
      std::sort(artists.begin() + 1, artists.end());
      last_artist.clear();
      last_album.clear();
      status = artist_error.empty()
                   ? "Select an artist to load albums and tracks"
                   : "Artists: " + artist_error;
      loading = false;
    });
  });

  std::atomic<bool> ticker_running{true};
  std::thread ticker([&] {
    while (ticker_running.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(180));
      if (ticker_running.load()) screen.PostEvent(Event::Custom);
    }
  });

  screen.Loop(view);
  ticker_running = false;
  if (ticker.joinable())
    ticker.join();
  if (loader.joinable())
    loader.join();
  for (auto &worker : workers)
    if (worker.joinable())
      worker.join();
  player.stop();
  return 0;
}

} // namespace sas
