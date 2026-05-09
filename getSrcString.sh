#!/bin/bash

for ARQ in `ls include/`; do
    echo "include/$ARQ"
    echo "==="
    cat "include/$ARQ"
    echo
done

for ARQ in `ls src/`; do
    echo $ARQ
    echo "==="
    cat "src/$ARQ"
    echo
done
