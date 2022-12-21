#!/bin/bash

# Invoked from cmake as mk-icns.sh /path/to/icon.svg /path/to/output.icns
svg="$1"
out="$2"
outdir="${out/%.icns/.iconset}"

set -e

# Apple's PNG encoding/decoding is buggy and likes to inject yellow lines, particularly for the
# smaller images.  This is apparently a known issue since macOS 11 that apple just doesn't give a
# shit about fixing (https://en.wikipedia.org/wiki/Apple_Icon_Image_format#Known_issues).
#
# So moral of the story: we have to arse around and edit the png to put a tranparent pixel in the
# bottom-left corner but that pixel *must* be different from the preceeding color, otherwise Apple's
# garbage breaks exposing the dumpster fire that lies beneath and drops the blue channel from the
# last pixel (or run of pixels, if they are the same color (ignoring transparency).  So, just to be
# consistent, we make *all* 4 corners transparent yellow, because it seems unlikely for our logo to
# have full-on yellow in the corner, and the color itself is irrelevant because it is fully
# transparent.
#
# Why is there so much broken, buggy crap in the macOS core???

no_r_kelly() {
    size=$1
    last=$((size - 1))
    for x in 0 $last; do
        for y in 0 $last; do
            echo -n "color $x,$y point "
        done
    done
}



mkdir -p "${outdir}"
for size in 32 64 128 256 512 1024; do
    # Yay Apple thanks for this utter trash OS.
    last=$((size - 1))
    convert -background none -resize "${size}x${size}" "$svg" \
        -fill '#ff00' -draw "$(no_r_kelly $size)" \
        -strip "png32:${outdir}/icon_${size}x${size}.png"
done


# Outputs the imagemagick -draw command to color the corner-adjacent pixels as half-transparent
# white.  We use this for the 16x16 (the others pick up corner transparency from the svg).
semitransparent_off_corners() {
    size=$1
    for x in 1 $((size - 2)); do
        for y in 0 $((size - 1)); do
            echo -n "color $x,$y point "
        done
    done
    for x in 0 $((size -1)); do
        for y in 1 $((size - 2)); do
            echo -n "color $x,$y point "
        done
    done
}

# For 16x16 we crop the image to 5/8 of its regular size before resizing which effectively zooms in
# on it a bit because if we resize the full icon it ends up a fuzzy mess, while the crop and resize
# lets us retain some detail of the logo.  (We don't do this for the 16x16@2x because that is really
# 32x32 where it retains enough detail).
convert -background none -resize 512x512 "$svg" -gravity Center -extent 320x320 -resize 16x16 \
    -fill '#ff00' -draw "$(no_r_kelly 16)" \
    -fill '#fff8' -draw "$(semitransparent_off_corners 16)" \
    -strip "png32:$outdir/icon_16x16.png"

# Create all the "@2x" versions which are just the double-size versions
rm -f "${outdir}/icon_*@2x.png"
mv "${outdir}/icon_1024x1024.png" "${outdir}/icon_512x512@2x.png"
for size in 16 32 128 256; do
    double=$((size * 2))
    ln -f "${outdir}/icon_${double}x${double}.png" "${outdir}/icon_${size}x${size}@2x.png"
done

iconutil -c icns "${outdir}"
