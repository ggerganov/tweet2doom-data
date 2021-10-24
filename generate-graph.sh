#!/bin/bash

wd=$(dirname $0)
cd $wd/
wd=$(pwd)

cd $wd

js="public/js/graph.js"
data="public/data"

echo "" > $js
echo "function graph_create() {" >> $js

for id in `ls $data/nodes` ; do
    if [ "$id" = "root" ] ; then continue ; fi
    read -r depth < $data/nodes/$id/depth
    depth=$((2*$depth + 0))

    if [ "$id" = "1444355917160534024" ] ; then
        # root node
        echo "nodes.push({ id: '$id', label: \"tweet2doom\", level: $depth, group: \"root\" });" >> $js
    else
        echo "nodes.push({ id: '$id', label: \"tweet2doom\", level: $depth, group: \"node\" });" >> $js
    fi
done

for id in `ls $data/processed` ; do
    read -r depth < $data/processed/$id/depth
    depth=$((2*$depth - 1))
    read -r username < $data/processed/$id/username

    echo "nodes.push({ id: '$id', label: \"$username\", level: $depth, group: \"command\" });" >> $js
done

echo "" >> $js

for id in `ls $data/processed` ; do
    read -r parent_id < $data/processed/$id/parent_id
    read -r child_id < $data/processed/$id/child_id
    echo "edges.push({ from: '$id', to: '$parent_id' });" >> $js
    echo "edges.push({ from: '$child_id', to: '$id' });" >> $js
done

echo "}" >> $js

cd $wd/layout/vis-network
node main.js
