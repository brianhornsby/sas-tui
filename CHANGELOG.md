# Changelog

All notable changes to sas-tui are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and releases use [Semantic Versioning](https://semver.org/).

## [0.1.0-rc2] - 2026-10-12

### Added

- Git-derived version reporting through `sas-tui --version`.
- The application version in the TUI title bar.
- Regression tests for playback progress rendering.
- A real now-playing screenshot in the README.

### Fixed

- A crash when a track reached 100% playback progress.
- Progress-bar rendering for zero, partial, complete, and over-duration playback.

## [0.1.0-rc1]

### Added

- Artist, album, and track browsing for Synology Audio Station.
- Queued playback through `ffplay`, with `mpv` fallback support.
- Artist-level track caching and keyboard navigation.
- JSON configuration, environment overrides, and CLI options.
- Linux/macOS builds, formatting, clang-tidy, sanitizer, coverage, and CodeQL
  checks in GitHub Actions.
- Tag-triggered Linux/macOS release packaging with checksums.

### Security

- Passwords remain environment-only.
- Synology session credentials and stream tokens are not written to disk.

[0.1.0-rc2]: https://github.com/brianhornsby/sas-tui/compare/v0.1.0-rc1...v0.1.0-rc2
[0.1.0-rc1]: https://github.com/brianhornsby/sas-tui/releases/tag/v0.1.0-rc1
