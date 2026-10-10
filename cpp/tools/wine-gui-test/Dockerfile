# Wine + headless X + a mingw-built guiprobe.exe, for checking that the
# Windows VST3 editor paints under Wine. See README.md.
FROM --platform=linux/amd64 debian:trixie

ENV DEBIAN_FRONTEND=noninteractive
RUN dpkg --add-architecture i386 && apt-get update && apt-get install -y --no-install-recommends \
        wine wine64 wine32:i386 \
        xvfb imagemagick python3 python3-pil unzip ca-certificates git \
        g++-mingw-w64-x86-64-posix \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch v3.8.0_build_66 \
        https://github.com/steinbergmedia/vst3_pluginterfaces.git /sdk/pluginterfaces

# Create the prefix at build time so each run starts from the same clean state.
# Skipping mono/gecko avoids the interactive installer prompts.
ENV WINEPREFIX=/wine WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
RUN wineboot -i && wineserver -w

# Without system fonts Wine's DirectWrite fails every text draw, which is not
# what users see. (Kept as its own layer so the slow layers above stay cached.)
RUN apt-get update && apt-get install -y --no-install-recommends \
        fonts-dejavu-core fonts-liberation2 \
    && rm -rf /var/lib/apt/lists/*

COPY guiprobe.cpp /probe/
RUN x86_64-w64-mingw32-g++-posix -std=c++17 -O2 -municode -static -I/sdk \
        /probe/guiprobe.cpp /sdk/pluginterfaces/base/funknown.cpp \
        /sdk/pluginterfaces/base/coreiids.cpp \
        -lole32 -luuid -luser32 -lgdi32 -o /probe/guiprobe.exe

COPY run-probe.sh analyze.py /probe/
ENTRYPOINT ["/probe/run-probe.sh"]
