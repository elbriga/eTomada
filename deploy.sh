#!/bin/bash
set -e
MANIFEST=eTomada.json

PASTA=data/www
OUT="$PASTA/js/eTomada.js"
F1="$PASTA/index.html"
F2="$PASTA/style.css"

DIRJS=javascript
ARQUIVOS=(
    eTomada.js
    api.js
    recurso.js
    reles.js
    sensores.js
    botoes.js
    config.js
    util.js
    regras.js
    umid.js
)
# Gerar bundle
{
    for arq in "${ARQUIVOS[@]}"; do
        echo "/* ===== $arq ===== */"
        cat "$DIRJS/$arq"
        printf '\n;\n\n'
    done
} > "$OUT"

gzip -9 -k -f "$OUT"
gzip -9 -k -f "$F1"
gzip -9 -k -f "$F2"

# Gerar o manifest
{
    echo '{"devices":[],"www":{"arquivos":{'

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
    echo "}}}"
} > "$MANIFEST"

PASTAPI=/root/nginx-docker/html/firmware
rsync -rav $PASTA/* pi:$PASTAPI/www/
scp "$MANIFEST" pi:$PASTAPI/

echo
echo "Bundle gerado"
echo "Arquivos: ${#ARQUIVOS[@]}"
du -h "$OUT"
du -h "$OUT.gz"
du -h "$F1.gz"
du -h "$F2.gz"
echo
