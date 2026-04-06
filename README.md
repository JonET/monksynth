# MonkSynth

A monophonic vocal synthesizer that sounds like a monk chanting. Built using formant-wave-function (FOF) synthesis, inspired by the classic [Delay Lama](http://www.audionerdz.nl/) VST plugin by AudioNerdz (2002).

<img src="docs/screenshot1.png" alt="MonkSynth running in Ableton Live 12 with the classic Delay Lama skin" width="600">

*MonkSynth v0.0.1-beta.1 in Ableton Live 12, with the classic skin imported from the original Delay Lama DLL*

## Features

- FOF synthesis engine producing realistic vocal formants
- XY pad for real-time pitch and vowel control
- Built-in stereo delay effect
- MIDI support: note on/off, pitch bend (vowel), CC1 (vibrato), CC5 (glide), CC7 (volume), CC12 (delay), CC13 (voice)
- ADSR envelope with configurable attack, decay, sustain, release
- Unison mode with up to 10 detuned voices and voice spread
- Theme system with right-click context menu for custom skins
- Import classic skin from the original Delay Lama DLL
- 5 factory presets
- VST3 plugin format (Windows, macOS, Linux)

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

## Repository Layout

```
dsp/
  voice.c, voice.h     FOF synthesis engine (formants, overlap-add grains)
  delay.c, delay.h     Stereo delay with feedback
  synth.c, synth.h     Public API: note stack, MIDI routing, gain staging
cpp/
  src/                  VST3 plugin shell (processor, controller, GUI)
  resources/            VSTGUI editor description and placeholder assets
  CMakeLists.txt        Build system (fetches VST3 SDK via FetchContent)
presets/                VST3 factory preset files
```

## Themes

MonkSynth ships without a built-in skin. On first launch, it shows a setup screen where you can import the classic look from the original Delay Lama DLL (available as freeware from [audionerdz.nl](http://www.audionerdz.nl/download.htm)).

You can also load custom themes via right-click on the plugin GUI. A theme folder contains a `theme.json` manifest and any combination of these PNG files (missing ones fall back to 1x1 placeholders):

- `background.png` — main background (360x510)
- `monk-strip.png` — animation sprite sheet (5x6 grid, 311x311 frames)
- `knob-left.png` / `knob-right.png` — rotary knob filmstrips (50x3000, 60 frames)
- `fader-down-large.png` / `fader-down-sm.png` / `fader-right-sm.png` — fader handles
- `info.png` — info overlay (253x275)

## Acknowledgments

- [Delay Lama](http://www.audionerdz.nl/) by AudioNerdz (2002) — the beloved freeware VST plugin that inspired this project
- Xavier Rodet (IRCAM) — formant-wave-function (FOF) synthesis technique
- [stb_image_write](https://github.com/nothings/stb) by Sean Barrett — single-header image writing (MIT / public domain)
- [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) by Steinberg — plugin framework (MIT)

## License

[MIT](LICENSE)
