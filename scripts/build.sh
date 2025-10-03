#!/bin/bash
# filepath: /home/alonso/Documents/simtemp/scripts/build.sh

set -e

cd "$(dirname "$0")/.."   # Go to project root

KERNEL_HEADERS="/lib/modules/$(uname -r)/build"

echo "[*] Checking for kernel headers..."
if [ ! -d "$KERNEL_HEADERS" ]; then
    echo "[ERROR] Kernel headers not found at $KERNEL_HEADERS"
    echo "Please install them with: sudo apt install linux-headers-$(uname -r)"
    exit 1
fi

echo "[*] Building kernel module..."
make -C kernel KDIR="$KERNEL_HEADERS" || { echo "[ERROR] Kernel module build failed"; exit 2; }

echo "[*] Building user CLI app..."
make -C user/cli || { echo "[ERROR] User CLI build failed"; exit 3; }

echo "[OK] Build completed successfully."