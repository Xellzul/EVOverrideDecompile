# syntax=docker/dockerfile:1.4
# EV Override 1.0.2 build kit (README.md). From the repository root:
#   docker build --platform linux/amd64 --build-context inputs=input-files -t evo-buildkit buildkit
# Unicorn runs the Mac-hosted Metrowerks compilers and linkers (tools/machost, tools/mpwhost), Wine the Windows-hosted
# ones (MWLinkPPC.exe, MWCPPC.exe); unpack/ lays out the rest from the inputs folder.
# The image holds the Metrowerks tools and libraries and your game copies: keep it private.
FROM --platform=linux/amd64 ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN dpkg --add-architecture i386 && apt-get update && apt-get install -y --no-install-recommends \
        wine wine32:i386 python3 python3-pip p7zip-full unar ca-certificates \
    && rm -rf /var/lib/apt/lists/*
RUN python3 -m pip install --no-cache-dir --break-system-packages unicorn==2.1.4

# a 32-bit Wine prefix, made once; no Mono or Gecko prompts
ENV WINEARCH=win32 WINEPREFIX=/root/.wine WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
RUN wineboot --init && wineserver -w

# the Metrowerks tools, libraries and headers, SAT and the game copies, unpacked from the inputs folder (unpack/)
COPY unpack/ /src/unpack/
RUN --mount=type=bind,from=inputs,target=/inputs python3 /src/unpack/unpack.py /inputs /opt/evo

WORKDIR /repo
CMD ["python3", "/repo/buildkit/build.py"]
