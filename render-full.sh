#!/bin/bash

if [ $# -lt 3 ] ; then
    echo "Usage: $0 id framerate nfreeze"
    exit 1
fi

wd=$(dirname $0)
cd $wd
wd=$(pwd)

dir_doomreplay="$wd/doomreplay"

rm -rf tmp
mkdir -p tmp

rm -rf .savegame
ln -sf $dir_doomreplay/.savegame .savegame

$dir_doomreplay/doomgeneric \
    -iwad $dir_doomreplay/doom1.wad \
    -input public/data/nodes/$1/history.txt \
    -output tmp/$1.mp4 \
    -nrecord 99999 \
    -framerate $2 \
    -nfreeze $3 \
    -render_frame \
    -render_input \
    -render_username || result=$?
