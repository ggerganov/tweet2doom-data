#!/bin/bash

src="../tweet2doom"
dst="./public"

rsync -rtvum --include='*parent_id'     --include='*/' --exclude='*' $src/processed/ $dst/data/processed/
rsync -rtvum --include='*child_id'      --include='*/' --exclude='*' $src/processed/ $dst/data/processed/
rsync -rtvum --include='*depth'         --include='*/' --exclude='*' $src/processed/ $dst/data/processed/
rsync -rtvum --include='*username'      --include='*/' --exclude='*' $src/processed/ $dst/data/processed/
rsync -rtvum --include='*input-cur.txt' --include='*/' --exclude='*' $src/processed/ $dst/data/processed/
rsync -rtvum --include='*frames_cur'    --include='*/' --exclude='*' $src/processed/ $dst/data/processed/
rsync -rtvum --include='*cmd.txt'       --include='*/' --exclude='*' $src/processed/ $dst/data/processed/

rsync -rtvum --include='*depth'         --include='*/' --exclude='*' $src/nodes/ $dst/data/nodes/
rsync -rtvum --include='*frames'        --include='*/' --exclude='*' $src/nodes/ $dst/data/nodes/
rsync -rtvum --include='*history.txt'   --include='*/' --exclude='*' $src/nodes/ $dst/data/nodes/
