#!/bin/bash

for ARQ in `ls include/`; do
    echo "include/$ARQ"
    echo "==="
    cat "include/$ARQ"
    echo
done

for ARQ in `ls src/`; do
    if [ -d "src/$ARQ" ]; then
        # 1 nivel de recursao manual
        for ARQ2 in `ls src/$ARQ/`; do
            echo "$ARQ/$ARQ2"
            echo "==="
            cat "src/$ARQ/$ARQ2"
            echo
        done
    else
        echo $ARQ
        echo "==="
        cat "src/$ARQ"
    fi
    echo
done
