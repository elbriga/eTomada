#!/bin/bash

URL="https://github.com/espressif/binutils-gdb/releases/download/esp-gdb-v17.1_20260402/riscv32-esp-elf-gdb-17.1_20260402-x86_64-linux-gnu.tar.gz"

TMP=$(mktemp -d)
curl -fL "$URL" -o "$TMP/gdb.tar.gz"

mkdir "$TMP/pkg"
tar -xzf "$TMP/gdb.tar.gz" -C "$TMP/pkg" --strip-components=1

cat > "$TMP/pkg/package.json" <<'EOF'
{
  "name": "tool-riscv32-esp-elf-gdb",
  "version": "17.1.0+20260402",
  "description": "Espressif RISC-V GDB"
}
EOF

pio pkg uninstall --global --tool "espressif/tool-riscv32-esp-elf-gdb"

pio pkg install --global \
  --tool "tool-riscv32-esp-elf-gdb=file://$TMP/pkg"

rm -rf "$TMP"
