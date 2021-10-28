#!/bin/bash

wd=$(dirname $0)
cd $wd
wd=$(pwd)

nseconds=20
nrecord=$((35*$nseconds))
nfreeze=$((35*10))

dir_doomreplay="$wd/doomreplay"
dir_tweet2doom="$wd/tweet2doom"

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
    if [ $frames -lt $((nrecord + 350)) ] ; then
        continue
    fi
    nodes="$nodes $i"
    num_nodes=$(($num_nodes + 1))
done

echo "Total nodes: $num_nodes"

id_selected=$(echo "$nodes" | tr " " "\n" | sort -R | tail -n 1)
id_parent=$(cat $id_selected/parent_id)
id_render=$(cat ../processed/$id_parent/parent_id)

frames_suggested=$(cat ../processed/$id_parent/frames_cur)

echo "Selected Id: $id_selected"
echo "Parent Id:   $id_parent"
echo "Render Id:   $id_render"

cd ../../../

rm -rf tmp
mkdir -p tmp

rm -rf .savegame
ln -sf $dir_doomreplay/.savegame .savegame

for i in $id_render ; do
    $dir_doomreplay/doomgeneric \
        -iwad $dir_doomreplay/doom1.wad \
        -input public/data/nodes/$i/history.txt \
        -output tmp/$i.mp4 \
        -nrecord $nrecord \
        -framerate 35 \
        -nfreeze $nfreeze \
        -render_frame \
        -render_input \
        -render_username || result=$?

    $dir_tweet2doom/parse-history public/data/nodes/$i/history.txt $nrecord 35 tmp/command_$i 0 1 > /dev/null
done

$dir_tweet2doom/parse-history public/data/nodes/$id_selected/history.txt $frames_suggested 1000 tmp/command_suggested 50 0 > /dev/null

j=0
cd tmp
for i in `ls *.mp4 | sort -R` ; do
   echo "file '$i'" >> list

   j=$(($j + 1))
   id=$(echo ${i%.*})

   echo "$id" > id

   for k in `seq 0 $((nseconds - 1))` ; do
       idx=$(( ($j - 1)*$nseconds + k ))

       ts_m=$(( 1000*($idx    ) ))
       te_m=$(( 1000*($idx + 1) ))

       ts_s=$(printf "%02d" $(( $ts_m/1000 )) )
       ts_m=$(printf "%03d" $(( $ts_m - ($ts_m/1000)*1000 )) )
       te_s=$(printf "%02d" $(( $te_m/1000 )) )
       te_m=$(printf "%03d" $(( $te_m - ($te_m/1000)*1000 )) )

       cmd=$(cat command_$id | head -n $(($k + 1)) | tail -n 1)

       echo "$(( $idx + 1 ))" >> subs.srt
       echo "00:00:$ts_s,$ts_m --> 00:00:$te_s,$te_m" >> subs.srt
       echo "$cmd" >> subs.srt
       echo "" >> subs.srt
   done
done

echo "$(( $idx + 2 ))" >> subs.srt
echo "00:00:20,500 --> 00:00:30,000" >> subs.srt
echo "Continue the game from here by replying to the quoted tweet below." >> subs.srt
echo "Let's play!" >> subs.srt

ffmpeg -f concat -safe 0 -i list -c copy lets-play.mp4
