#!/bin/bash

for ARQ in `ls data/`; do
    if [ "${ARQ:0:7}" == "favicon" ]; then
        continue
    fi

    if [ -d "data/$ARQ" ]; then
        # 1 nivel de recursao manual
        for ARQ2 in `ls data/$ARQ/`; do
            if [ -d "data/$ARQ/$ARQ2" ]; then
                # 2 nivel de recursao manual
                for ARQ3 in `ls data/$ARQ/$ARQ2/`; do
                    echo "/$ARQ/$ARQ2/$ARQ3"
                    echo "==="
                    cat "data/$ARQ/$ARQ2/$ARQ3"
                    echo
                done
            else
                echo "/$ARQ/$ARQ2"
                echo "==="
                cat "data/$ARQ/$ARQ2"
            fi
            echo
        done
    else
        echo "/$ARQ"
        echo "==="
        cat "data/$ARQ"
    fi
    echo
done
