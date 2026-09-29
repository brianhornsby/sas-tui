# sas-tui

An unofficial Tokyo Night terminal music player for Synology Audio Station,
with a library-first workflow inspired by musikcube.

The current version connects to a DSM Web API endpoint, browses artists, albums,
and tracks, and starts playback through FFmpeg's `ffplay` (with mpv as a fallback). The code is
split into model, backend, player, and UI/application layers.

## Requirements

- C++20 compiler
- CMake 3.20+
- libcurl development files
- `ffplay` on `PATH` (included with FFmpeg), or `mpv` as a fallback
- A Synology NAS with Audio Station enabled

The build fetches FTXUI and nlohmann/json with CMake `FetchContent`.

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

## Run

Set the NAS URL and credentials in the environment. Use an HTTPS URL where
possible:

```sh
export SAS_TUI_URL=https://nas.example.com:5001
export SAS_TUI_USER=music-player
export SAS_TUI_PASSWORD='your-password'
./build/sas-tui
```

For a local NAS using a self-signed DSM certificate, you can temporarily
disable TLS certificate verification:

```sh
export SAS_TUI_INSECURE_TLS=1
```

Use this only on a trusted network. A valid certificate or trusted local CA is
preferred.

The account should have only the Audio Station permissions it needs. Credentials
are never written to disk by sas-tui.

Selecting an artist or album and pressing Enter creates a temporary FFmpeg
concat playlist and plays the filtered tracks sequentially. The playlist is
removed when playback is stopped or replaced.

Keys:

```text
←/→   switch pane       Tab    next pane
↑/↓   navigate list     Enter  play selected song
Enter in Artists/Albums queues the filtered tracks
Space pause/resume      s      stop
[ / ] previous/next
Esc/q quit
```

The app currently uses the DSM API to discover Audio Station endpoints and
supports the common `SYNO.AudioStation.Song` listing API. API versions and
parameters vary between DSM releases, so the client discovers supported
versions at startup.
