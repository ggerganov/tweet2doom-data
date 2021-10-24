#!/bin/bash

wd=$(dirname $0)
cd $wd/
wd=$(pwd)

while true ; do
    sleep 1800

    echo "Doing update `date` ..."

    cd $wd/public/

    git checkout master
    git pull --rebase

    cd $wd

    ./sync.sh > log-sync.txt

    cd $wd/public/

    num_new=$(git ls-files --others --exclude-standard data/nodes | wc -l)
    if [ "$num_new" -eq 0 ] ; then
        echo "Nothing to update"
        continue;
    fi

    cd $wd

    ./generate-graph.sh

    cd $wd/public/

    git add data/*
    git add js/graph.js
    git add json/*

    git commit -m "New states added [`date`]"
    git push
done
