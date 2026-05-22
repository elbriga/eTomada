#!/bin/bash

for ARQ in `ls data/`; do
    if [ "${ARQ:0:7}" == "favicon" ]; then
        continue
    fi

    if [ -d "data/$ARQ" ]; then
        # 1 nivel de recursao manual
        for ARQ2 in `ls data/$ARQ/`; do
            echo "/$ARQ/$ARQ2"
            echo "==="
            cat "data/$ARQ/$ARQ2"
            echo
        done
    else
        echo "/$ARQ"
        echo "==="
        cat "data/$ARQ"
    fi
    echo
done
