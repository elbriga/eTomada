#!/bin/bash

PASTA=../data/www

echo "{"

first=true

for file in $PASTA/* $PASTA/js/*; do
    [ -f "$file" ] || continue

    hash=$(sha256sum "$file" | awk '{print $1}')
    path="/${file#$PASTA/}"

    if [ "$first" = true ]; then
        first=false
    else
        echo ","
    fi

    printf '  "%s": "%s"' "$path" "$hash"
done

echo
echo "}"