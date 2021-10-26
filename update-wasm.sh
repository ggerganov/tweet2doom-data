#!/bin/bash

wd=$(dirname $0)
cd $wd
wd=$(pwd)

cd $wd

cp -v build-em/bin/t2d-explorer.data ./public/
cp -v build-em/bin/t2d-explorer.js   ./public/
cp -v build-em/bin/t2d-explorer.wasm ./public/

cp -v build-em/bin/t2d-explorer-extra/style.css        ./public/
cp -v build-em/bin/t2d-explorer-extra/index.html       ./public/
cp -v build-em/bin/t2d-explorer-extra/gghelpers.js     ./public/js/
cp -v build-em/bin/t2d-explorer-extra/background-0.png ./public/images/
