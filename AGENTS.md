# sas-tui Agent Guide

## Project overview

`sas-tui` is a C++20 FTXUI terminal player for Synology Audio Station. The
application loads artists first, then albums and tracks for the selected artist.
Playback uses FFmpeg's `ffplay` when available and supports an `mpv` fallback
for both single-track and queued playback.

## Repository layout

- `src/app.cpp` — FTXUI layout, selection, caching, and user input.
- `src/config.cpp` — JSON config, CLI parsing, and environment precedence.
- `src/synology_client.cpp` — Synology DSM/Audio Station HTTP API client.
- `src/player.cpp` — external player process management.
- `include/sas/model.hpp` — shared song and album models.
- `include/sas/*.hpp` — public interfaces.
- `CMakeLists.txt` — build configuration and fetched dependencies.
- `include/sas/config.hpp` — non-secret runtime configuration model.
- `.github/workflows/ci.yml` — Linux/macOS build checks for pushes and pull requests.

## Build and verify

```sh
cmake -S . -B build
cmake --build build -j2
```

Always rebuild after changing C++ sources. Run `git diff --check` and scan
changed files for secrets. Optional quality targets are `format`,
`format-check`, and `tidy`; ASan and UBSan can be enabled with
`SAS_TUI_ENABLE_ASAN=ON` and `SAS_TUI_ENABLE_UBSAN=ON`. There is no automated
integration test suite because it requires a reachable Synology Audio Station
instance. GitHub Actions performs Release builds on Linux and macOS for every
push and pull request, plus formatting, clang-tidy, and an Ubuntu sanitizer
build. The workflow can also be started manually.

## Runtime configuration

Connection details may be supplied through
`~/.config/sas-tui/config.json` (or `$XDG_CONFIG_HOME/sas-tui/config.json`),
command-line options, or environment variables. The precedence is defaults,
config file, environment, then CLI. `SAS_TUI_CONFIG` and `--config PATH` select
an alternate JSON file.
Use `--help` for the complete option list. `SAS_TUI_PASSWORD` remains
environment-only; never put it in a config file or command line. The config
file may contain `url`, `user`, `player` (`auto`, `ffplay`, or `mpv`),
`insecure_tls`, and a `ui` object with pane percentage settings. Artist and
album widths must total 80% or less to preserve the Tracks pane.

`SAS_TUI_URL`, `SAS_TUI_USER`, `SAS_TUI_PASSWORD`, `SAS_TUI_PLAYER`, and
`SAS_TUI_INSECURE_TLS=1` are supported environment overrides. CLI flags also
include `--url`, `--user`, `--player`, `--insecure-tls`, and `--secure-tls`.
The application prints a warning when insecure TLS is enabled.

Never hard-code passwords, session IDs, Synology tokens, NAS addresses, or
stream URLs containing bearer tokens. Do not commit `.env` files, build output,
logs, or generated playlist files. Queued playback keeps stream URLs in memory.

## UI and playback conventions

- Keep Artists, Albums, and Tracks keyboard-navigable with Tab and arrow keys.
- Keep the focused pane visibly highlighted.
- Preserve the minimum Albums height and Now Playing beneath Tracks.
- Keep network work off the UI thread and retain artist-level track caching.
- Keep status and error messages in the footer rather than replacing Tracks.
- Preserve `/` search for artists, albums, and loaded tracks.
- Preserve `c` queue stop, `r` repeat, and `z` shuffle controls.
- Ensure queued playback can be stopped cleanly during shutdown.
- Preserve the `mpv` fallback when changing playback behavior.

## Change workflow

1. Make the smallest change that satisfies the request.
2. Build with the commands above.
3. Run `git diff --check` and scan changed files for secrets.
4. Summarize behavior changes and verification in the handoff.
