#!/bin/bash

# Invoked from cmake as mk-icns.sh /path/to/icon.svg /path/to/output.icns
svg="$1"
out="$2"
outdir="${out/%.icns/.iconset}"

set -e

mkdir -p "${outdir}"
for size in 16 32 64 128 256 512 1024; do
    convert -background none -resize "${size}x${size}" "$svg" -strip "png32:${outdir}/icon_${size}x${size}.png"
done
mv "${outdir}/icon_1024x1024.png" "${outdir}/icon_512x512@2x.png"
for size in 16 32 128 256; do
    double=$((size * 2))
    cp "${outdir}/icon_${double}x${double}.png" "${outdir}/icon_${size}x${size}@2x.png"
done

iconutil -c icns "${outdir}"
