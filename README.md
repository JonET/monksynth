
<div align="right">
  <details>
    <summary >🌐 Language</summary>
    <div>
      <div align="center">
        <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=en">English</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=zh-CN">简体中文</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=zh-TW">繁體中文</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=ja">日本語</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=ko">한국어</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=hi">हिन्दी</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=th">ไทย</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=fr">Français</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=de">Deutsch</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=es">Español</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=it">Italiano</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=ru">Русский</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=pt">Português</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=nl">Nederlands</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=pl">Polski</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=ar">العربية</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=fa">فارسی</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=tr">Türkçe</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=vi">Tiếng Việt</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=id">Bahasa Indonesia</a>
        | <a href="https://openaitx.github.io/view.html?user=JonET&project=monksynth&lang=as">অসমীয়া</
      </div>
    </div>
  </details>
</div>

# MonkSynth

[![Build](https://github.com/JonET/monksynth/actions/workflows/build.yml/badge.svg)](https://github.com/JonET/monksynth/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/JonET/monksynth?include_prereleases)](https://github.com/JonET/monksynth/releases)
[![License](https://img.shields.io/github/license/JonET/monksynth)](LICENSE)

A monophonic vocal synthesizer that sounds like a monk chanting. Built using formant-wave-function (FOF) synthesis, inspired by the classic [Delay Lama](http://www.audionerdz.nl/) VST plugin by AudioNerdz (2002).

**[Download the latest release](https://github.com/JonET/monksynth/releases)** — available for Windows, macOS, and Linux.

<img src="docs/screenshot1.png" alt="MonkSynth running in Ableton Live 12 with the classic Delay Lama theme" width="600">

*MonkSynth v0.0.1-beta.1 in Ableton Live 12, with the classic theme imported from the original Delay Lama DLL*

## Features

- FOF synthesis engine producing realistic vocal formants
- XY pad for real-time pitch and vowel control
- Built-in stereo delay effect
- MIDI support: note on/off, pitch wheel, CC1 (vibrato), CC5 (glide), CC7 (volume), CC12 (delay), CC13 (voice)
- Automatable **Pitch Bend** parameter (±12 semitones). The hardware pitch wheel is routable to either Vowel (Classic / Delay Lama compat, the default) or Pitch via right-click → Pitch Bend
- ADSR envelope with configurable attack, decay, sustain, release
- Unison mode with up to 10 detuned voices and voice spread
- Theme system with right-click context menu for custom themes
- Import classic theme from the original Delay Lama DLL
- 5 factory presets
- VST3 plugin format (Windows, macOS, Linux) and Audio Unit (macOS)

## Building

### Prerequisites

- CMake 3.20+
- C/C++ compiler (MSVC, GCC, or Clang)

### Build

```bash
cd cpp
cmake -B build
cmake --build build --config Release --target MonkSynth
```

The VST3 SDK is fetched automatically by CMake. The built plugin is placed in your system VST3 directory.

### macOS Audio Unit

To also build the AU plugin, install the [AudioUnit SDK](https://github.com/apple/AudioUnitSDK) and configure with:

```bash
cmake -B build -G Xcode -DSMTG_AUDIOUNIT_SDK_PATH=/path/to/AudioUnitSDK
cmake --build build --config Release --target MonkSynth-au
```

### DSP unit tests

The pure-C DSP layer (`dsp/`) has a small unit test suite exercising ADSR envelope boundaries, the note stack, unison detune math, pitch-bend propagation, and delay-line feedback stability. Tests are opt-in so they don't affect normal plugin builds:

```bash
cd cpp
cmake -B build-tests -DMONKSYNTH_BUILD_TESTS=ON
cmake --build build-tests --config Release
ctest --test-dir build-tests --output-on-failure
```

CI runs the test suite on the Linux job before packaging each release, so any DSP regression blocks the build.

## Installation

- **macOS:** Run the `.pkg` installer — installs both VST3 and AU plugins
- **Windows:** Run the `.exe` installer — installs the VST3 plugin
- **Linux:** Extract and copy `MonkSynth.vst3` to `~/.vst3/`

### Linux compatibility

The Linux build is verified on each release to load cleanly under strict loader semantics (Bitwig-style `dlopen(RTLD_NOW)`) on these distro families:

- Ubuntu 22.04 / 24.04 LTS (and derivatives: Linux Mint, Pop!_OS, Elementary, KDE neon)
- Debian 12 (and derivatives: KX Studio, AV Linux, MX Linux)
- Fedora (latest)
- Arch Linux (and derivatives: Manjaro, EndeavourOS, CachyOS)

If your distro isn't listed it most likely still works — these are smoke-tested in CI to catch the missing-shared-library class of bug, not an exhaustive support claim. The plugin is built on Ubuntu 22.04 (glibc 2.35), so any distro with glibc ≥ 2.35 should be compatible. Reports from other distros are welcome via [GitHub Issues](https://github.com/JonET/monksynth/issues).

## Themes

MonkSynth ships without a built-in theme. On first launch, it shows a setup screen where you can import the classic look from the original Delay Lama DLL (available as freeware from [audionerdz.nl](http://www.audionerdz.nl/download.htm)).

You can also load custom themes via right-click on the plugin GUI. A theme folder contains a `theme.json` manifest and any combination of these PNG files (missing ones fall back to 1x1 placeholders):

- `background.png` — main background (360x510)
- `monk-strip.png` — animation sprite sheet (5x6 grid, 311x311 frames)
- `knob-left.png` / `knob-right.png` — rotary knob filmstrips (50x3000, 60 frames)
- `fader-down-large.png` / `fader-down-sm.png` / `fader-right-sm.png` — fader handles
- `info.png` — info overlay (253x275)

**Looking for fresh default themes to ship with the plugin.** If you design a theme you're proud of, open a PR — I'd love to include contributed themes in the next release. The right-click menu has an "Open Themes Folder" item that reveals where themes live on disk.

## Translations

The plugin UI (setup screen, info overlay, right-click menu, and DLL-importer error messages) is available in English, Japanese, and Korean. The language auto-detects from your OS locale; you can override it via right-click → Language.

**Japanese and Korean translations were generated by a large language model as a starting point.** Native-speaker contributions are very welcome — please open a PR editing `cpp/src/strings_ja.h` or `cpp/src/strings_ko.h`. Every string is indexed by the `StringId` enum in `cpp/src/i18n.h`; keep entries in the same order and leave any you're unsure about as empty strings to fall back to English.

Parameter names (shown in your DAW's automation lanes) stay English on purpose — tutorials, presets, and community discussion all assume the English names.

## Code Signing Policy

Free code signing provided by [SignPath.io](https://about.signpath.io/), certificate by [SignPath Foundation](https://signpath.org/).

The Windows VST3 plugin and installer are signed as part of the release build in GitHub Actions. Signing requests are submitted to SignPath only for tagged releases built from this repository, and each request is manually approved in the SignPath UI before the certificate is applied.

| Privileged role | Signer |
|-----------------|--------|
| Author          | [Jonathan Taylor](https://github.com/JonET) |
| Reviewer        | [Jonathan Taylor](https://github.com/JonET) |
| Approver        | [Jonathan Taylor](https://github.com/JonET) |

### Privacy Policy

This program will not transfer any information to other networked systems unless specifically requested by the user or the person installing or operating it.

## Acknowledgments

- [Delay Lama](http://www.audionerdz.nl/) by AudioNerdz (2002) — the beloved freeware VST plugin that inspired this project
- Xavier Rodet (IRCAM) — formant-wave-function (FOF) synthesis technique
- [stb_image_write](https://github.com/nothings/stb) by Sean Barrett — single-header image writing (MIT / public domain)
- [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) by Steinberg — plugin framework (MIT)
- [SignPath Foundation](https://signpath.org/) — free Windows code signing for open source projects

## License

[MIT](LICENSE)
