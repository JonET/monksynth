# Changelog

All notable changes to MonkSynth will be documented in this file.

## [1.0.1] - 2026-09-21

### Fixed
- The 1.0.0 macOS binaries only loaded on macOS 26.5 or newer, even though the installer accepted 10.15. CMake creates the deployment-target cache entry during `project()` (defaulting to the build machine's OS under Xcode), so the project's later setting was ignored and each release inherited the CI runner's version. The target is now set before `project()`, the minimum is macOS 11, and CI fails if a release binary reports anything else.
- The Audio Unit was not listed by Ableton Live although it passed `auval` and loaded in Logic and GarageBand. Live requires the AudioComponents name to be in `Manufacturer: Product` form and silently skips components without the prefix; ours was the bare product name (#35).
- Every macOS bundle reported the wrong version: the VST3 bundle and `moduleinfo.json` said 0.0.1 and the AU said 0.2.0 with a component version of 1, whatever the release. The tag version now feeds `project()` and a configured AU plist, so hosts and the AU cache see the real version and no longer treat every release as identical.
- Memory-safety pass following a sanitizer and code audit of the DSP core, the plugin layer and the DLL importer. None of these crashed in normal use, but each was real:
  - Sample rates above 192 kHz sized the grain past the fixed voice tables and wrote into the next voice's state. The grain is now clamped to the table size, and rates of zero, NaN or outside 1 kHz to 768 kHz fall back to 44.1 kHz instead of spinning the vibrato phase wrap forever.
  - NaN parameter values passed the range clamps and, on x86, turned into INT_MIN table indices in the vowel lookup and the delay read. Every setter's clamp is now NaN-safe and pitch inputs are clamped to the audible range.
  - The delay read index could round to exactly one past the end of the line at certain rate and sample-rate pairs. The index is now split from the fraction before wrapping.
  - reset() re-arms the output gain smoothers so a non-finite value can't persist across a reset; a null check in set_sample_rate came one line too late.
  - Deferred UI work (theme switch, menu actions, overlay close, folder import) used VSTGUI's Call::later, which can't be cancelled, so a closure could fire after the host had released the editor. Deferred work is now owned by the controller and cancelled when the editor closes; theme selection is persisted immediately and only the view rebuild is deferred. The pitch-bend spring-back timer is also stopped when the editor closes.
  - setComponentState read an uninitialized float on a truncated state stream; it now stops at the short read and leaves the remaining parameters at their defaults, matching the processor.
  - The classic DLL importer trusted the palette size in the file's bitmap header. A file of the right size with a forged CRC32 could make it read far past the buffer. Header fields are now read unsigned and every offset is checked against the file before decoding.
- New DSP unit tests cover 384 kHz, zero and NaN sample rates, NaN parameters, the delay wrap and reset; the suite runs clean under AddressSanitizer and UndefinedBehaviorSanitizer.

## [1.0.0] - 2026-09-13

First stable release. The beta line has been in use since April; this drops the beta tag and gathers everything since 0.2.0-beta.15 below.

### Added
- Right-click menu now lists every theme installed in the user themes folder under "Theme: <current>", so switching themes no longer requires the file chooser. "Load Theme..." is still there for themes elsewhere on disk.
- `themes/` directory in the repo for community-contributed themes, starting with "Smiley Face..." by gav (#20). Smiley Face ships inside the plugin bundle (`Contents/Resources/themes/`), appears in the theme menu on every platform, and is offered on the first-run setup screen as a one-click alternative to importing the classic theme. The classic import remains the primary onboarding path.
- The five factory presets other than Monastary now carry the same plug-in name/vendor/category metadata Monastary had, so hosts that group presets by metadata list all six together. Sound settings are unchanged. Contributed by CosmicScribe64 (#30).
- "About Theme..." in the right-click menu shows the active theme's credits: name, author, description and a clickable link, read from new optional `description` and `url` fields in `theme.json`. The imported classic theme credits AudioNerdz and links to audionerdz.nl.

### Fixed
- Importing the classic theme in Logic Pro and GarageBand failed with "Could not open the selected file" for a DLL in Downloads (or Desktop/Documents). macOS folder privacy blocks Apple's AU hosting process from reading files chosen in the file panel, and there is no way for the plugin to obtain that access. A DLL copied into the MonkSynth themes folder is now imported automatically (the setup screen watches the folder while it's open), the error shows the OS reason plus that instruction, and the setup screen's "Open themes folder" link gets you there. The DLL can also be dragged and dropped onto the setup screen. Reported against Logic 12 on macOS 26 (#27).
- The VST3 zip downloads now include the factory presets, and the README says where to put them. Linux had no way to get them before since it has no installer (#21).
- Notes, pitch bend, XY-pad moves and parameter automation are now applied at their exact sample position within the audio block instead of at the block boundary. Previously every note began up to one buffer early, by an amount that varied note to note, which was audible as sloppy timing at large buffer sizes and visible in bounced audio. Affected every host; reported against Cubase 14 (#22).
- Unison and output gain changes are smoothed over a fixed ~5 ms rather than over one audio callback, so the ramp no longer depends on buffer size.
- Blocks longer than 8192 samples (some offline renders) are rendered in full instead of leaving the tail of the buffer unwritten.
- `Controller::beginEdit`/`endEdit` no longer declare `PLUGIN_API`, matching the SDK's `EditController` signature. Fixes compilation of 32-bit Windows builds. Contributed by Arakula (#26).

## [0.2.0-beta.15] - 2026-05-14

### Added
- Windows VST3 plugin and installer are now code-signed via [SignPath Foundation](https://signpath.org/). Signing happens automatically for tagged releases in CI; signing requests require manual approval in the SignPath UI before the workflow proceeds.

## [0.2.0-beta.14] - 2026-05-06

### Fixed
- Linux VST3 no longer crashes the plugin host process when the editor opens in Bitwig. Patched VSTGUI's `x11frame.cpp` to call `setRunLoop` before `RunLoop::init()` -- the upstream order tried to register an event handler against the LinuxFactory's runloop before the fallback wired one in from the plugFrame, causing a NULL deref in any host that doesn't supply `Steinberg::Linux::IRunLoop` via `setHostContext` (Bitwig). REAPER was unaffected because it sets the host context up front. Refs #13.
- Linux VST3 no longer hits a `_get_screen_index` cairo assertion when an editor is closed and reopened. Patched VSTGUI's `RunLoop::Impl::exit()` to skip `xcb_disconnect` -- cairo's process-global xcb_connection_t cache can't be invalidated, so a recycled connection pointer on next attach aliased the freed setup. Costs ~1 fd per init/exit cycle.

### Added
- pluginval (Tracktion) now runs in CI against every Linux build at strictness level 10. Validates audio processing across 15 sample-rate/block-size combinations, parameter automation, state save/load, fuzz parameters, and editor lifecycle. The Bitwig and cairo bugs above were both surfaced by this step.

## [0.2.0-beta.13] - 2026-05-06

### Fixed
- Linux VST3 no longer fails to load on distros without libthai installed (e.g. Linux Mint, Arch). Pango pulls libthai for Thai word-breaking; the static build now bundles libthai 0.1.30 and libdatrie 0.2.14 so the symbols resolve regardless of host environment. Symptom previously seen as `undefined symbol: th_uni2tis` in Bitwig (which uses `dlopen(RTLD_NOW)`), or as a delayed segfault when the editor opened in hosts using lazy symbol resolution. Refs #13.
- Linux VST3 no longer requires `libbz2.so.1.0` at runtime, which Fedora and other RPM-based distros don't ship (they have `libbz2.so.1` only). Disabled bzip2 support in the bundled freetype since BZIP2-compressed PCF fonts are vanishingly rare. May resolve the load-time segfault under Carla on Debian reported in #1.

### Added
- `cpp/scripts/verify-load-linux.sh`: spins up containers for Ubuntu 22.04 / 24.04, Debian 12, Fedora, and Arch and verifies the Linux VST3 loads cleanly under strict `dlopen(RTLD_NOW)` semantics (the same loader behavior Bitwig uses). Runs in CI on every push so this class of cross-distro library bug fails the build instead of escaping to a user.
- README section listing supported Linux distro families and the glibc 2.35 minimum.

## [0.2.0-beta.12] - 2026-05-05

### Fixed
- Linux VST3 no longer fails to load in Bitwig (and other hosts) with `undefined symbol: g_list_model_get_type`. Pango 1.56 introduced GListModel-based font enumeration that depends on libgio-2.0 (`g_list_model_get_type`, `g_io_error_quark`, `g_list_model_get_n_items`, `g_list_model_items_changed`), but the static-link group only included libglib-2.0 and libgobject-2.0. Adding libgio-2.0 and libgmodule-2.0 resolves the four undefined symbols. Thanks to @skei for the report (#13).

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
