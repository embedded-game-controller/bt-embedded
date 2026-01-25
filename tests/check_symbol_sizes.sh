#! /bin/sh

set -e

BIN0="$1"
BIN1="$2"
shift 2
SYMBOLS="$@"

ALL_SYMBOLS0="$(objdump -t "$BIN0" | grep  '.text' | sed 's/.*text\s*\(\w*\)\s*\(\w*\)/\1 \2/')"
ALL_SYMBOLS1="$(objdump -t "$BIN1" | grep  '.text' | sed 's/.*text\s*\(\w*\)\s*\(\w*\)/\1 \2/')"

for SYMBOL in "$SYMBOLS"
do
    SIZE0="$(echo "$ALL_SYMBOLS0" | grep "$SYMBOL" | cut -d' ' -f1)"
    SIZE1="$(echo "$ALL_SYMBOLS1" | grep "$SYMBOL" | cut -d' ' -f1)"
    if [ -z "$SIZE0" -o -z "$SIZE1" ]; then
        echo "Error: symbol $SYMBOL missing ($SIZE0 and $SIZE1)"
        exit 1
    fi
    SIZE0=$(printf "%d" "0x$SIZE0")
    SIZE1=$(printf "%d" "0x$SIZE1")
    if [ "$SIZE0" -ge "$SIZE1" ]; then
        echo "Error: symbol $SYMBOL in $BIN0 is not smaller! ($SIZE0 and $SIZE1)"
    fi
done
