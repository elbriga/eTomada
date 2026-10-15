#!/bin/bash
set -e

OUT="data/www/js/eTomada.js"
F1="data/www/index.html"
F2="data/www/style.css"

DIR=javascript
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
        cat "$DIR/$arq"
        printf '\n;\n\n'
    done
} > "$OUT"

gzip -9 -k -f "$OUT"
gzip -9 -k -f "$F1"
gzip -9 -k -f "$F2"

echo
echo "Bundle gerado"
echo "Arquivos: ${#ARQUIVOS[@]}"
du -h "$OUT"
du -h "$OUT.gz"
echo
