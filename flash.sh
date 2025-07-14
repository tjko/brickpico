#!/bin/sh
#
# Flash Fanpico firmware using picoprobe
#

file=$1
[ -n "$file" ] || file=build/brickpico.elf

if [ ! -s "$file" ]; then
    echo "File not found: $file"
    exit 1
fi

echo "Flashing $file ..."

openocd -f interface/cmsis-dap.cfg \
	-c "adapter speed 5000" \
	-f target/rp2040.cfg \
	-c "program ${file} verify reset exit"


# eof :-)
