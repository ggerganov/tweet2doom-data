#!/bin/bash

wd=$(dirname $0)
cd $wd
wd=$(pwd)

dir_tweet2doom="$wd/tweet2doom"

git pull --rebase
git submodule update --remote -- public

./lets-play.sh

id=$(cat ./tmp/id)
cmd=$(cat ./tmp/command_suggested)

/usr/bin/node $dir_tweet2doom/post/post.js 0 "🕹️ Let's play

Reply to the quoted tweet to continue the game.

Suggested command:
/play $cmd

Tweet chain:
https://twitter.com/tweet2doom/status/$id

Node:
https://tweet2doom.github.io/?f=$id

The game so far:" ./tmp/lets-play.mp4 ./tmp/subs.srt

echo $? > ./tmp/post.code
