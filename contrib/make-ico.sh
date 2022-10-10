#!/bin/bash

# Invoked from cmake as make-ico.sh /path/to/icon.svg /path/to/output.ico
svg="$1"
out="$2"
outdir="$out.d"

set -e

sizes=(16 24 32 40 48 64 96 192 256)
outs=""

mkdir -p "${outdir}"
for size in "${sizes[@]}"; do
    outf="${outdir}/${size}x${size}.png"
    if [ $size -lt 32 ]; then
        # For 16x16 and 24x24 we crop the image to 3/4 of its regular size before resizing and make
        # it all white (instead of transparent) which effectively zooms in on it a bit because if we
        # resize the full icon it ends up a fuzzy mess, while the crop and resize lets us retain
        # some detail of the logo.
        convert -background white -resize 512x512 "$svg" -gravity Center -extent 320x320 -resize ${size}x${size} -strip "png32:$outf"
    else
        convert -background transparent -resize ${size}x${size} "$svg" -strip "png32:$outf"
    fi
    outs="-r $outf $outs"
done

icotool -c -b 32 -o "$out" $outs
