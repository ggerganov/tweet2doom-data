#!/bin/bash

wd=$(dirname $0)
cd $wd
wd=$(pwd)

dir_doomreplay="$wd/doomreplay"

cd public/data/nodes

nodes_all="$(ls)"
nodes=""
num_nodes=0
for i in $nodes_all ; do
    read -r depth < ${i}/depth
    read -r frames < ${i}/frames
    if [ $depth -lt 3 ] ; then
        continue
    fi
    if [ $frames -lt 1050 ] ; then
        continue
    fi
    nodes="$nodes $i"
    num_nodes=$(($num_nodes + 1))
done

echo "Total nodes: $num_nodes"

nodes_render=$(echo "$nodes" | tr " " "\n" | sort -R | tail -n 10)

cd ../../../

rm -rf tmp
mkdir -p tmp

rm -rf .savegame
ln -sf $dir_doomreplay/.savegame .savegame

for i in $nodes_render ; do
    $dir_doomreplay/doomgeneric \
        -iwad $dir_doomreplay/doom1.wad \
        -input public/data/nodes/$i/history.txt \
        -output tmp/$i.mp4 \
        -nrecord 350 \
        -framerate 60 \
        -nfreeze 10 \
        -render_frame \
        -render_input \
        -render_username || result=$?
done

cd tmp
for i in `ls | sort -R` ; do
   echo "file '$i'" >> list
done

ffmpeg -f concat -safe 0 -i list -c copy highlights.mp4
