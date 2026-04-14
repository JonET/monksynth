# Changelog

All notable changes to MonkSynth will be documented in this file.

## [Unreleased]

## [0.2.0-beta.5] - 2026-04-13

### Added
- MIDI CC and pitch bend support: pitch bend maps to vowel, CC1=vibrato, CC5=glide, CC7=volume, CC12=delay, CC13=voice character
- XY pad performances can now be recorded as DAW automation (XY Note, XY Vowel, XY Pitch parameters)
- Vowel smoothing on XY pad with 10-tick linear ramp matching the original Delay Lama
- Factory presets from the original Delay Lama: Rabten, Dorje, Ngawang, Jamyang, Tinley
- Plugin state save/load (presets and DAW session recall now work)
- Presets included in Windows and macOS installers

### Changed
- XY pad note sustain now matches original: sound plays until both mouse and MIDI keys are released
- XY pad pitch/vowel movements slide smoothly instead of snapping, matching the original's portamento behavior
- XY pad faders are now read-only indicators showing smoothed state
- MIDI portamento uses per-sample constant-rate slew at ±12 semitones/sec, matching the original's formula
- Renamed "Voice" parameter to "HeadSize"; added original parameter units (Hours, Vowel, dB, cm)

### Fixed
- Fixed stack overflow crash when dragging the XY pad pitch slider while a note is held
- Fixed setParamNormalized re-entrancy causing infinite recursion in some hosts

## [0.2.0-beta.4] - 2026-04-08

### Changed
- Linux: statically link all GUI dependencies (cairo, pango, harfbuzz, fontconfig, freetype, glib, etc.) into the plugin binary — eliminates crashes caused by shared library conflicts with DAWs and other plugins
- Linux: use DejaVu Sans font instead of Arial (not available on most Linux distros)
- macOS: use Helvetica font instead of Arial

### Removed
- Linux: removed bundled .so files from .vst3 directory (no longer needed)

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

[Unreleased]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.4...HEAD
[0.2.0-beta.4]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.3...v0.2.0-beta.4
[0.2.0-beta.3]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.2...v0.2.0-beta.3
[0.2.0-beta.2]: https://github.com/JonET/monksynth/compare/v0.0.1-beta.1...v0.2.0-beta.2
[0.0.1-beta.1]: https://github.com/JonET/monksynth/releases/tag/v0.0.1-beta.1
