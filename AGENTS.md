# sas-tui Agent Guide

## Project overview

`sas-tui` is a C++20 FTXUI terminal player for Synology Audio Station. The
application loads artists first, then albums and tracks for the selected artist.
Playback uses FFmpeg's `ffplay` when available and supports an `mpv` fallback
for both single-track and queued playback.

## Repository layout

- `src/app.cpp` — FTXUI layout, selection, caching, and user input.
- `src/synology_client.cpp` — Synology DSM/Audio Station HTTP API client.
- `src/player.cpp` — external player process management.
- `include/sas/model.hpp` — shared song and album models.
- `include/sas/*.hpp` — public interfaces.
- `CMakeLists.txt` — build configuration and fetched dependencies.

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
instance.

## Runtime configuration

Connection details are supplied through `SAS_TUI_URL`, `SAS_TUI_USER`,
`SAS_TUI_PASSWORD`, and optionally `SAS_TUI_INSECURE_TLS=1` for trusted
networks with self-signed certificates.

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
