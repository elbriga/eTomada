Import("env")

from pathlib import Path

gdb = Path.home() / ".platformio/packages/tool-riscv32-esp-elf-gdb/bin/riscv32-esp-elf-gdb"

env.Replace(GDB=str(gdb))

print(f"DEBUG GDB: {gdb}")
