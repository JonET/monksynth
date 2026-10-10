# wine-gui-test

Checks that the Windows VST3 editor paints under Wine (issues #36 and #40)
without a Linux box or a DAW.

- `guiprobe.cpp` is a minimal VST3 host. It loads only the edit controller,
  opens the editor, then sets note-on and sweeps Vowel and Pitch Bend through
  `setParamNormalized`, which is the redraw path a DAW drives.
- `run-probe.sh` runs inside the container: Xvfb, Wine, guiprobe, and an X
  screenshot of each phase (static, sweep, settle).
- `analyze.py` crops the screenshots to the editor, reports the fraction of
  black pixels per frame and prints a verdict: `BLACK EDITOR` (#36),
  `BLACK REGIONS` (#40) or `OK`. It also writes `contact-sheet.png`.

## Running

```sh
cpp/tools/wine-gui-test/run.sh release:v1.0.1
cpp/tools/wine-gui-test/run.sh fix=path/to/MonkSynth-VST3-Windows.zip
```

Zips are the `MonkSynth-VST3-Windows` CI artifact or release asset. Results go
to `cpp/build/wine-gui-test/<label>/`. `WINE_OVERRIDES=dcomp=d run.sh ...`
adds Wine DLL overrides for experiments.

v1.0.1 shows `BLACK REGIONS`: the editor paints at rest, then the monk view
turns black once vowel and pitch start changing, matching #40.

## Apple Silicon

Wine crashes under Rosetta (`rosetta error: invalid gdt selector`), so the
container needs an emulated x86_64 Docker VM. One-time setup:

```sh
brew install qemu lima-additional-guestagents
colima start -p wine --arch x86_64 --vm-type qemu --cpu 6 --memory 6
docker context use colima   # keep your usual VM as the default
```

`run.sh` uses the `colima-wine` context automatically when it exists. The
first image build takes a while under emulation; later runs take a minute or
two each.
