#!/bin/bash

src="../tweet2doom"
dst="./public"

rsync -rtvum \
    --include='*parent_id' \
    --include='*child_id' \
    --include='*depth' \
    --include='*username' \
    --include='*input-cur.txt' \
    --include='*frames_cur' \
    --include='*cmd.txt' \
    --include='*/' --exclude='*' $src/processed/ $dst/data/processed/

rsync -rtvum \
    --include='*depth' \
    --include='*frames' \
    --include='*parent_id' \
    --include='*history.txt' \
    --include='*/' --exclude='*' $src/nodes/ $dst/data/nodes/
