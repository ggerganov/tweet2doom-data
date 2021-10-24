#!/bin/bash

wd=$(dirname $0)
cd $wd
wd=$(pwd)

cd $wd/public/data/processed

max_depth=$(for i in `find -name *depth` ; do cat $i ; done | sort -n | tail -n 1)
num_players=$(for i in `find -name *username` ; do cat $i ; echo ""; done | sort | uniq | wc -l)

cd $wd/public/data/nodes
max_frames=$(for i in `find -name *frames` ; do cat $i ; done | sort -n | tail -n 1)
num_nodes=$(($(ls -l | wc -l) - 1))

echo "Max depth:   $max_depth"
echo "Max frames:  $max_frames"
echo "Num players: $num_players"
echo "Num nodes:   $num_nodes"
