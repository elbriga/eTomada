
# WWW
rsync -rav /home/gabriel/Documents/PlatformIO/Projects/eTomada/data/www/* pi:/root/nginx-docker/html/firmware/www/
cd firmware-server/ && bash geraManifest.sh && cd -

# eTomada.json
scp /home/gabriel/Documents/PlatformIO/Projects/eTomada/firmware-server/eTomada.json pi:/root/nginx-docker/html/firmware/

# Firmware
sudo scp .pio/build/lolin/firmware.bin pi:/root/nginx-docker/html/firmware/lolin32_R6S4B1_1.3.9.bin

# Log Server (eTomada Server!)
sudo rsync -rav /home/gabriel/Documents/PlatformIO/Projects/eTomada/log-server/* pi:/opt/etomada-log-server/
# dentro do raspberry:
docker stop etomada-log-server
cd /opt/etomada-log-server
docker compose up -d --build

# Umidificador!
curl -v -X PUT "http://192.168.1.151/api/setUmidificador" -H "Content-Type: application/json" -d '{"estado":0}'

# Backtrace
~/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line -pfiaC -e .pio/build/lolin/firmware.elf 0x4008428d:0x3ffb2c50 0x4008f309:0x3ffb2c70 ...

# sonoff
# Avahi?
https://www.sigmdel.ca/michel/ha/sonoff/sonoff_mini_en.html
avahi-browse -t _ewelink._tcp --resolve
# API
curl -v http://192.168.1.115:8081/zeroconf/info -H "Content-Type: application/json" -d '{"deviceid":"1000c8797e","data":{}}'


# LITE!
curl -F "firmware=@.pio/build/dev/firmware.bin" http://192.168.1.225/api/ota_flash
