#!/bin/bash

HOST=$1

uso() {
	echo "Uso: ./sobeFirmware.sh HOST"
	echo
	exit 1
}

[[ -z "$HOST" ]] && uso

FW=.pio/build/$HOST/firmware.bin
if [ ! -f "$FW" ]; then
	echo "Arquivo de firmware nao encontrado: $FW"
	echo "Compile o firmware antes de subir"
	exit 2
fi
# verificar se o arquivo tem mais de 30 minutos, se sim, avisar que o firmware pode estar desatualizado
if [ $(find "$FW" -mmin +30 | wc -l) -gt 0 ]; then
	echo "Arquivo de firmware desatualizado: $FW"
	echo "Compile o firmware antes de subir"
	exit 3
fi

TAM=$(stat -c%s $FW)
SHA=$(sha256sum $FW | awk '{print $1}')

URL="http://$HOST/api/ota?tamanho=$TAM&sha=$SHA"

CMD="curl -X POST -F \"firmware=@$FW\" '$URL'"

echo "============================"
echo "Subindo FW: $FW"
echo "URL: $URL"
echo "============================"
echo $CMD
echo

curl -X POST -F "firmware=@$FW" "$URL"

echo
echo
