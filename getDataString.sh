#!/bin/bash

for ARQ in `ls data/`; do
    if [ "${ARQ:0:7}" == "favicon" ]; then
        continue
    fi

    echo "/$ARQ"
    echo "==="
    cat "data/$ARQ"
    echo
done
