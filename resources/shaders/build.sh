#!/bin/sh
# Compiles the shaders for every graphics API Qt Quick uses (Vulkan, OpenGL,
# OpenGL ES, Direct3D, Metal). The .qsb files are committed so no build
# needs Qt Shader Tools. Needs qsb from Qt 6 (e.g. /usr/lib/qt6/bin/qsb).
set -e
cd "$(dirname "$0")"
QSB=${QSB:-$(command -v qsb || echo /usr/lib/qt6/bin/qsb)}
for f in *.vert *.frag; do
    # The current format; Qt 6.6 and later read it. Vertex shaders of scene
    # graph materials also need the batchable variant (-b).
    case "$f" in *.vert) batch=-b ;; *) batch= ;; esac
    "$QSB" $batch --glsl "100 es,120,150" --hlsl 50 --msl 12 -O -o "$f.qsb" "$f"
done
