#!/bin/bash

# 检查已上传文件的格式

if [ -z "$1" ]; then
    echo "Usage: $0 <file_path>"
    echo "Example: $0 uploads/libs/mylib_20260411_104446_215.so"
    exit 1
fi

FILE="$1"

if [ ! -f "$FILE" ]; then
    echo "Error: File not found: $FILE"
    exit 1
fi

echo "=== File Format Check ==="
echo "File: $FILE"
echo "Size: $(stat -c%s "$FILE" 2>/dev/null || stat -f%z "$FILE" 2>/dev/null) bytes"
echo ""

echo "First 16 bytes (hex):"
hexdump -C "$FILE" | head -n 1

echo ""
echo "First 16 bytes (ASCII):"
head -c 16 "$FILE" | cat -v

echo ""
echo ""

# 检查魔数
MAGIC=$(head -c 8 "$FILE" | od -An -tx1 | tr -d ' \n')

if [ "${MAGIC:0:14}" = "7f454c46" ]; then
    echo "Format: ELF"
    
    # 读取 e_type (偏移 16, 2 bytes, little-endian)
    E_TYPE=$(od -An -t x2 -j 16 -N 2 "$FILE" | tr -d ' ')
    
    case "$E_TYPE" in
        "0100")
            echo "Type: ET_REL (Relocatable file / .o)"
            echo "Expected extension: .o"
            ;;
        "0200")
            echo "Type: ET_EXEC (Executable)"
            echo "Expected extension: (none)"
            ;;
        "0300")
            echo "Type: ET_DYN (Shared object / .so)"
            echo "Expected extension: .so"
            ;;
        "0400")
            echo "Type: ET_CORE (Core file)"
            echo "Expected extension: .core"
            ;;
        *)
            echo "Type: Unknown ($E_TYPE)"
            ;;
    esac
    
elif [ "${MAGIC:0:16}" = "213c617263683e0a" ]; then
    echo "Format: AR (Archive)"
    echo "Type: Static library"
    echo "Expected extension: .a"
    
    echo ""
    echo "Archive members:"
    ar -t "$FILE" 2>/dev/null || echo "(ar command not available)"
    
else
    echo "Format: Unknown"
    echo "Magic bytes: $MAGIC"
fi

echo ""
echo "=== Check Complete ==="
