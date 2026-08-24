
# WWW
rsync -rav /home/gabriel/Documents/PlatformIO/Projects/eTomada/data/www/* pi:/root/nginx-docker/html/firmware/www/
cd firmware-server/ && bash geraManifest.sh && cd -

# eTomada.json
scp /home/gabriel/Documents/PlatformIO/Projects/eTomada/firmware-server/eTomada.json pi:/root/nginx-docker/html/firmware/

# Firmware
scp .pio/build/lolin/firmware.bin pi:/root/nginx-docker/html/firmware/lolin32_R6S4B1_1.3.9.bin

# Umidificador!
curl -v -X PUT "http://192.168.1.151/api/setUmidificador" -H "Content-Type: application/json" -d '{"estado":0}'

# Backtrace
~/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line -pfiaC -e .pio/build/lolin/firmware.elf 0x4008428d:0x3ffb2c50 0x4008f309:0x3ffb2c70 ...
