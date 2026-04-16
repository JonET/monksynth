# Changelog

All notable changes to MonkSynth will be documented in this file.

## [Unreleased]

## [0.2.0-beta.11] - 2026-04-15

### Fixed
- Monk animation now reflects the current hold-vs-idle state when the editor is opened while a MIDI note is already held. Previously the monk stayed in idle until the note was released and re-pressed, because `kNoteActive` is only pushed on edges — the controller now syncs from the stored parameter value when the monk view is constructed.
- Local universal-binary builds no longer fail to link x86_64 because of the `monk_dsp` static library defaulting to the host arch. `smtg_target_setup_universal_binary` is now applied so it follows the same arch policy as the plugin target. CI builds were unaffected (they set `CMAKE_OSX_ARCHITECTURES` globally).

### Changed
- UI terminology: standardized on "theme" instead of mixing "theme" and "skin" across the setup screen, right-click menu, and translations (EN/JA/KO). Thanks to @nonno2010sw-ux for the suggestion in #10.

## [0.2.0-beta.10] - 2026-04-15

### Fixed
- Hardware pitch wheel coupling in Both / Both (Inverted Vowel) modes no longer bleeds into the in-plugin Pitch Bend slider or DAW automation lane. The wheel is now routed through a hidden `kPitchWheelRaw` parameter so the processor can fan out to pitch bend + vowel without entangling user-facing controls. Dragging the Pitch Bend slider or automating it in the DAW moves pitch bend independently, as expected.
- Loading presets saved by beta.9 (21 params) into beta.10 (22 params) no longer fails — `setState` handles short reads gracefully and leaves new params at their defaults.

## [0.2.0-beta.9] - 2026-04-15

### Added
- Vowel and Pitch sliders inside the plugin window are now real parameter controls — FL Studio users can right-click them to create automation clips or link to a controller. Previously they were read-only indicators with no control-tag, so host right-click menus had nothing to grab.
- **Pitch Bend** submenu gains two new routing modes: `Both (Pitch + Vowel)` and `Both (Pitch + Inverted Vowel)`. In Both modes, moving the hardware pitch wheel drives pitch bend and vowel simultaneously (same direction or opposite). Classic (0.0) and Pitch (1.0) still map to the same saved-state values, so existing sessions and presets load unchanged.
- Pitch bend slider springs back to center on release with a 180 ms cubic ease-out, matching how a hardware pitch wheel behaves. The spring-back is also recorded into automation as a fresh edit gesture.

### Fixed
- Loading a preset via the host's preset arrows no longer leaves the synth stuck on the previous sound until a control is nudged — `setState` now pushes all parameter values to the DSP. The attack/decay/release scaling (3.0 → 5.0) was also unified with the runtime path so initial load matches the rest of the session.
- Releasing the XY pad while a MIDI note is still held no longer kicks the monk into the idle shuffle animation. The monk's hold-vs-idle state is now driven from the processor's combined (MIDI || XY pad) signal instead of reacting to pad release directly.

### Removed
- Dead `kXYPitch` private display parameter and its audio-thread writeback (the pitch slider now represents pitch bend directly, so the smoothed XY-pad pitch no longer needs a separate feedback channel).

## [0.2.0-beta.8] - 2026-04-15

### Added
- `Pitch Bend` parameter (±12 semitones, automatable) that actually bends the pitch of held notes, independent of the existing `Vowel` parameter
- Right-click → **Pitch Bend** submenu with `Classic (Vowel)` and `Pitch` options; toggle controls where the hardware MIDI pitch wheel lands. Classic is the default and preserves Delay Lama compatibility. The host is notified via `restartComponent(kMidiCCAssignmentChanged)` so the switch takes effect without reloading the plugin.
- Minimal DSP unit test suite (`cpp/tests/test_voice.c`, `test_synth.c`, `test_delay.c`) covering ADSR envelope boundaries, note stack LIFO, unison detune math, pitch-bend propagation, and delay-line feedback stability. Opt-in via `-DMONKSYNTH_BUILD_TESTS=ON`; runs on the Linux CI job before packaging.

### Changed
- DSP sources refactored into a `monk_dsp` static library so the plugin and the unit tests can link against the same objects without duplicating the source list.
- Removed dead `monk_synth_pitch_bend` DSP function that was never called and just redirected to `set_vowel`.

## [0.2.0-beta.7] - 2026-04-14

### Added
- Japanese and Korean UI localization for the setup overlay, info overlay, right-click context menu, and DLL-importer error messages (~35 strings). Parameter names and units stay English so DAW automation lanes, presets, and tutorials continue to match. Translations are LLM-generated and have not yet been reviewed by a native speaker — corrections are very welcome (see `cpp/src/strings_ja.h` and `strings_ko.h`).
- Automatic UI language detection from OS locale (`GetUserDefaultLocaleName` on Windows, `CFLocale` on macOS, `$LANG`/`$LC_*` on Linux), with manual override via right-click → Language submenu (Auto / English / 日本語 / 한국어)
- CJK-capable font fallback: Yu Gothic UI / Malgun Gothic on Windows, Hiragino Sans / Apple SD Gothic Neo on macOS, Noto Sans CJK on Linux
- "Create your own theme" contribution section in both the setup and info overlays, with a clickable "Open themes folder" link — actively looking for fresh default themes to ship with future releases
- "Open Themes Folder" item in the right-click menu that reveals the user theme directory in the system file browser
- Language preference persisted in `config.json` alongside the existing theme path

## [0.2.0-beta.6] - 2026-04-14

### Fixed
- Fixed crash when importing Delay Lama DLL on systems with non-ASCII file paths (e.g. Japanese Windows usernames)
- Use wide-character file I/O on Windows to avoid ANSI code page conversion errors
- Wrap all file selector callbacks in exception handlers to prevent unhandled C++ exceptions from crashing the host

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

[Unreleased]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.8...HEAD
[0.2.0-beta.8]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.7...v0.2.0-beta.8
[0.2.0-beta.7]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.6...v0.2.0-beta.7
[0.2.0-beta.6]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.5...v0.2.0-beta.6
[0.2.0-beta.5]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.4...v0.2.0-beta.5
[0.2.0-beta.4]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.3...v0.2.0-beta.4
[0.2.0-beta.3]: https://github.com/JonET/monksynth/compare/v0.2.0-beta.2...v0.2.0-beta.3
[0.2.0-beta.2]: https://github.com/JonET/monksynth/compare/v0.0.1-beta.1...v0.2.0-beta.2
[0.0.1-beta.1]: https://github.com/JonET/monksynth/releases/tag/v0.0.1-beta.1
