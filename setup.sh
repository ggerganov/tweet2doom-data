#!/bin/bash

wd=$(dirname $0)
cd $wd
wd=$(pwd)

cd $wd

git submodule update --init

ln -sfnv ../tweet2doom ./tweet2doom
ln -sfnv ../doomreplay/doomgeneric ./doomreplay

cd $wd/layout/vis-network
rm -rf node_modules
npm install
# patch the stupid fucking module ... wasted so much time with this shit
cp patch/vis-data.js node_modules/vis-data/peer/umd/vis-data.js

cd $wd
