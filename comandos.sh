
# WWW
rsync -rav /home/gabriel/Documents/PlatformIO/Projects/eTomada/data/www/* pi:/root/nginx-docker/html/firmware/www/
cd firmware-server/ && bash geraManifest.sh && cd -

# eTomada.json
scp /home/gabriel/Documents/PlatformIO/Projects/eTomada/firmware-server/eTomada.json pi:/root/nginx-docker/html/firmware/

# Firmware
scp .pio/build/lolin/firmware.bin pi:/root/nginx-docker/html/firmware/lolin32_R6S4B1_1.3.9.bin

# Umidificador!
curl -v -X PUT "http://192.168.1.151/api/setUmidificador" -H "Content-Type: application/json" -d '{"estado":0}'
