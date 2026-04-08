# Changelog

All notable changes to MonkSynth will be documented in this file.

## [Unreleased]

## [0.2.0-beta.3] - 2026-04-07

### Added
- Info screen accessible via "?" button — shows version, license, creator, and link to GitHub
- Clickable URL on the setup screen (audionerdz.nl download link)
- Linux: bundle shared libraries into .vst3 for portability (no more manual dependency installs)
- Linux: build on Ubuntu 22.04 (glibc 2.35) for broader distro compatibility

### Fixed
- Build now defaults to Release when no `CMAKE_BUILD_TYPE` is specified, fixing build failures with the VST3 SDK
- Linux: UI event handling after skin import (deferred UI recreation)

## [0.2.0-beta.2] - 2026-04-05

### Added
- macOS Audio Unit (AU) plugin format
- macOS `.pkg` installer (installs both VST3 and AU)
- macOS code signing and notarization
- Windows `.exe` installer (Inno Setup)

### Fixed
- Knob animation frame count calculation
- macOS file dialog crash in Ableton Live (deferred `NSOpenPanel` opening)
- AU plugin registration and bundle structure

## [0.0.1-beta.1] - 2026-04-04

### Added
- Initial release
- FOF synthesis engine with realistic vocal formants
- XY pad for real-time pitch and vowel control
- Built-in stereo delay effect
- MIDI support (note on/off, pitch bend, CC1/5/7/12/13)
- ADSR envelope
- Unison mode (up to 10 voices with detune and spread)
- Theme system with right-click context menu
- Import classic skin from original Delay Lama DLL
- 5 factory presets
- CI/CD with cross-platform builds (Windows, macOS, Linux)

[Unreleased]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.3...HEAD
[0.2.0-beta.3]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.2...v0.2.0-beta.3
[0.2.0-beta.2]: https://github.com/JonET/monksynth/compare/v0.0.1-beta.1...v0.2.0-beta.2
[0.0.1-beta.1]: https://github.com/JonET/monksynth/releases/tag/v0.0.1-beta.1
