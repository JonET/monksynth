#!/bin/bash
# Runs inside the container: unpack one plugin zip, open its editor under Wine
# with guiprobe, screenshot each phase, then analyze the frames.
#   run-probe.sh <plugin.zip> <out-dir>
set -euo pipefail

zip=$1
out=$2
mkdir -p "$out"
rm -f "$out"/*.png "$out"/*.log "$out"/summary.txt

work=$(mktemp -d)
unzip -q "$zip" -d "$work"
vst3=$(find "$work" -maxdepth 3 -type d -name 'MonkSynth.vst3' | head -1)
if [ -z "$vst3" ]; then
    echo "no MonkSynth.vst3 in $zip" >&2
    exit 1
fi

# Pick a bundled theme so the editor shows the monk rather than the setup
# screen (the classic theme needs the Delay Lama DLL).
cfg="$WINEPREFIX/drive_c/users/$(whoami)/AppData/Roaming/MonkSynth"
mkdir -p "$cfg"
echo '{"themePath": "bundled:smileyface"}' > "$cfg/config.json"

Xvfb :99 -screen 0 1280x960x24 -nolisten tcp &
export DISPLAY=:99
sleep 1

log="$out/probe.log"
: >"$log"
wine /probe/guiprobe.exe "$(winepath -w "$vst3")" >"$log" 2>"$out/wine.log" &
probe=$!

wait_for() {
    for _ in $(seq 600); do
        grep -q "$1" "$log" && return 0
        grep -q '^FAIL' "$log" && { cat "$log" >&2; exit 1; }
        kill -0 "$probe" 2>/dev/null || { echo "guiprobe exited early" >&2; cat "$log" >&2; exit 1; }
        sleep 0.1
    done
    echo "timed out waiting for $1" >&2
    exit 1
}

shot() { import -window root "$out/$1.png"; }

# guiprobe holds each phase for a fixed time (3s static, 10s sweep, 5s
# settle). Screenshots are slow under emulation, so sweep shots stop as soon
# as the phase ends rather than at a fixed count.
wait_for 'PHASE static'
sleep 2
shot 00-static
wait_for 'PHASE sweep'
i=0
while ! grep -q 'PHASE settle' "$log" && [ $i -lt 12 ]; do
    i=$((i + 1))
    sleep 0.3
    shot "$(printf '1%02d-sweep' $i)"
done
wait_for 'PHASE settle'
sleep 2
shot 20-settle
grep -q 'PHASE done' "$log" && echo "warning: settle shot taken after the window closed" >&2
wait_for 'PHASE done'
wait "$probe" || true

python3 /probe/analyze.py "$out" | tee "$out/summary.txt"
