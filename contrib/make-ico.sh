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
        # For 16x16 and 24x24 we crop the image to 2/3 of its regular size make it all white
        # (instead of transparent) to zoom in on it a bit because if we resize the full icon to the
        # target size it ends up a fuzzy mess, while the crop and resize lets us retain some detail
        # of the logo.
        rsvg-convert -b white \
            --page-height $size --page-width $size \
            -w $(($size*3/2)) -h $(($size*3/2)) --left " -$(($size/4))" --top " -$(($size/4))" \
            "$svg" >"$outf"
    else
        rsvg-convert -b transparent -w $size -h $size "$svg" >"$outf"
    fi
    outs="-r $outf $outs"
done

icotool -c -b 32 -o "$out" $outs
