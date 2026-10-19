#!/bin/bash
set -e
MANIFEST=eTomada.json

PASTA=data/www
OUT=$PASTA/index.html

BUNDLE=app.js.tmp
DIRJS=frontend/js
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
# Gerar bundle JS
{
    for arq in "${ARQUIVOS[@]}"; do
        echo "/* ===== $arq ===== */"
        cat "$DIRJS/$arq"
        printf '\n;\n\n'
    done
} > "$BUNDLE"

# Gerar index.html com CSS e JS embutidos
CSS_FILE="frontend/style.css" JS_FILE="$BUNDLE" \
perl -0777 -pe '
    BEGIN {
        local $/;

        open my $fh, "<", $ENV{CSS_FILE} or die $!;
        $css = <$fh>;
        close $fh;

        open $fh, "<", $ENV{JS_FILE} or die $!;
        $js = <$fh>;
        close $fh;
    }

    s/INSERIR_CSS_AQUI/$css/g;
    s/INSERIR_JS_AQUI/$js/g;
' "frontend/index-tpl.html" > "$OUT"

rm "$BUNDLE"

gzip -9 -k -f "$OUT"
rm "$OUT"

gzip -9 -k -f "frontend/portal.html"
mv "frontend/portal.html.gz" $PASTA


# Gerar o manifest
{
    echo '{"devices":[],"www":{"arquivos":{'
    first=true
    for file in $PASTA/*; do
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
    echo "}}}"
} > "$MANIFEST"

# Jogar no raspberry
PASTAPI=/root/nginx-docker/html/firmware
rsync -rav $PASTA/* pi:$PASTAPI/www/
scp "$MANIFEST" pi:$PASTAPI/

echo
echo "$PASTA gerado"
echo "manifest [$MANIFEST] gerado"
echo "sync pi"
echo
for arq in $PASTA/*; do
    du -h "$arq"
done
du -h "$PASTA"
echo
