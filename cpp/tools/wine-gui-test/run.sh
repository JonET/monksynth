#!/bin/bash
# Check that Windows VST3 builds paint under Wine, from a Mac or Linux host.
#
#   cpp/tools/wine-gui-test/run.sh [label=]MonkSynth-VST3-Windows.zip ...
#   cpp/tools/wine-gui-test/run.sh release:v1.0.1
#
# Results land in cpp/build/wine-gui-test/<label>/ (summary.txt,
# contact-sheet.png, per-frame crops, probe and wine logs).
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
results=$(cd "$here/../.." && pwd)/build/wine-gui-test
image=monksynth-wine-gui-test

# Wine does not run under Rosetta, so on Apple Silicon this needs an emulated
# x86_64 Docker VM (see README.md). Use it when it exists.
if [ -z "${DOCKER_CONTEXT:-}" ] && docker context inspect colima-wine >/dev/null 2>&1; then
    export DOCKER_CONTEXT=colima-wine
fi

if [ $# -eq 0 ]; then
    sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi

docker build --platform linux/amd64 -q -t "$image" "$here" >/dev/null

failed=0

for arg in "$@"; do
    if [[ $arg == release:* ]]; then
        tag=${arg#release:}
        label=$tag
        mkdir -p "$results/downloads/$tag"
        zip="$results/downloads/$tag/MonkSynth-VST3-Windows.zip"
        [ -f "$zip" ] || gh release download "$tag" -R JonET/monksynth \
            -p MonkSynth-VST3-Windows.zip -D "$results/downloads/$tag"
    elif [[ $arg == *=* ]]; then
        label=${arg%%=*}
        zip=${arg#*=}
    else
        zip=$arg
        label=$(basename "$zip" .zip)
    fi

    zip=$(cd "$(dirname "$zip")" && pwd)/$(basename "$zip")
    mkdir -p "$results/$label"
    echo "== $label"
    # WINE_OVERRIDES adds Wine DLL overrides, e.g. WINE_OVERRIDES=dcomp=d to
    # see how a build behaves when DirectComposition is unavailable.
    # INSTANCES sets how many editors guiprobe opens side by side (default 2).
    status=0
    docker run --rm --platform linux/amd64 \
        -e WINEDLLOVERRIDES="mscoree,mshtml=${WINE_OVERRIDES:+;$WINE_OVERRIDES}" \
        -e INSTANCES="${INSTANCES:-2}" \
        -v "$zip:/in/plugin.zip:ro" -v "$results/$label:/out" \
        "$image" /in/plugin.zip /out >"$results/$label/run.log" 2>&1 || status=$?
    grep -E 'VERDICT|black|rect|FAIL|exited|timed out|Traceback|Error' "$results/$label/run.log" || true
    if [ $status -ne 0 ]; then
        echo "FAILED ($label exited $status; see $results/$label/run.log)"
        failed=1
    fi
done

echo "results: $results"
exit $failed
