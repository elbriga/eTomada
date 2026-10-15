#!/bin/bash

for ARQ in data/config/* data/www/*; do
    EXT=${ARQ##*.}
    [ "$EXT" == "ico" ] && continue
    [ "$EXT" == "gz" ] && continue

    echo "/$ARQ"
    echo "==="
    cat "$ARQ"
    echo
done
