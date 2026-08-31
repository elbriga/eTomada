#!/bin/bash

HOST=$1
ENDPOINT=$2
JSON=$3

uso() {
    echo "USO: ./api.sh HOST [ENDPOINT] [JSON]"
    echo
    exit 1
}
[[ -z "$HOST" ]] && uso

[[ -z "$ENDPOINT" ]] && ENDPOINT=getSnapshot

URL="http://$HOST/api/$ENDPOINT"

if [ ! -z "$JSON" ]; then
    echo "POST para [$URL]"
    curl -v -X POST "$URL" -H "Content-Type: application/json" -d "$JSON"
else
    echo "GET para [$URL]"
    curl -v "$URL"
fi

echo
echo

