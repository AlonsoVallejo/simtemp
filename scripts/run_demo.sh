#!/bin/bash
# filepath: /home/alonso/Documents/simtemp/scripts/run_demo.sh

set -e

cd "$(dirname "$0")/.."

KMOD="nxp_simtemp.ko"
KMOD_PATH="kernel/$KMOD"
DEVNODE="/dev/simtemp"
CLI="./user/cli/simtemp_cli"

# 1. Build first (optional, comment out if not desired)
# ./scripts/build.sh

# 2. Insert kernel module
echo "[*] Inserting kernel module..."
sudo insmod "$KMOD_PATH" || { echo "[ERROR] insmod failed"; exit 1; }

# 3. Wait for /dev/simtemp to appear
for i in {1..10}; do
    if [ -e "$DEVNODE" ]; then break; fi
    sleep 0.2
done
if [ ! -e "$DEVNODE" ]; then
    echo "[ERROR] /dev/simtemp not found after insmod"
    sudo rmmod nxp_simtemp || true
    exit 2
fi

# 4. Configure device via sysfs
echo "[*] Configuring sampling_ms=100, threshold_mC=44100"
echo 100 | sudo tee /sys/class/simtemp_class/simtemp/sampling_ms > /dev/null || true
echo 44100 | sudo tee /sys/class/simtemp_class/simtemp/threshold_mC > /dev/null || true

# 5. Run CLI test mode
echo "[*] Running CLI test mode..."
$CLI --test
CLI_RESULT=$?

# 6. Remove kernel module
echo "[*] Removing kernel module..."
sudo rmmod nxp_simtemp || { echo "[ERROR] rmmod failed"; exit 3; }

if [ $CLI_RESULT -eq 0 ]; then
    echo "[OK] Demo completed successfully."
    exit 0
else
    echo "[ERROR] CLI test failed."
    exit 4
fi