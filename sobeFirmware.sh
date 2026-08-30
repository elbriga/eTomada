#!/bin/bash

FW=$1
HOST=$2

uso() {
	echo "Uso: ./sobeFirmware.sh ARQUIVO.bin HOST"
	echo
	exit 1
}

[[ -z "$FW" || -z "$HOST" ]] && uso
[[ ! -f "$FW" ]] && uso

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

