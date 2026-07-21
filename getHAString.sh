#!/bin/bash
DIR=HomeAssistant/config/custom_components/etomada
for ARQ in `ls $DIR/`; do
    echo "$DIR/$ARQ"
    echo "==="
    cat "$DIR/$ARQ"
    echo
done
