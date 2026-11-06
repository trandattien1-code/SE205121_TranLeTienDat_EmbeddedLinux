
#!/bin/sh

MODULE="/counter_driver.ko"
DEVICE="/dev/counter"
PROC="/proc/counter_info"
FAIL=0

check_equal() {
    if [ "$2" = "$3" ]; then
        echo "PASS: $1"
    else
        echo "FAIL: $1 (expected=$3, actual=$2)"
        FAIL=$((FAIL + 1))
    fi
}

echo "================================="
echo " ASSIGNMENT 01 - TASK 2 TEST"
echo "================================="

# 1. Load module
echo "[1] Loading counter driver"

if grep -q '^counter_driver ' /proc/modules; then
    echo "FAIL: module already loaded"
    exit 1
fi

insmod "$MODULE" || exit 1

# 2. Check device
echo "[2] Checking device node"

if [ ! -c "$DEVICE" ]; then
    echo "FAIL: /dev/counter missing"
    rmmod counter_driver
    exit 1
fi

ls -l "$DEVICE"

MAJOR=$(ls -l "$DEVICE" | awk '{print $5}')
check_equal "Major number" "$MAJOR" "10,"

# 3. Test auto-increment
echo "[3] Testing auto-increment"

A=$(cat "$DEVICE")
B=$(cat "$DEVICE")
C=$(cat "$DEVICE")

check_equal "First read" "$A" "0"
check_equal "Second read" "$B" "1"
check_equal "Third read" "$C" "2"

# Statistics before write
echo ""
echo "===== PROC STATS BEFORE WRITE ====="
cat "$PROC"

# 4. Set counter = 100
echo ""
echo "[4] Testing SET counter = 100"

if ! echo 100 > "$DEVICE"; then
    echo "FAIL: write 100"
    FAIL=$((FAIL + 1))
fi

A=$(cat "$DEVICE")
check_equal "Set 100" "$A" "100"

# Statistics after write
echo ""
echo "===== PROC STATS AFTER WRITE ====="
cat "$PROC"

# 5. Reset counter = 0
echo ""
echo "[5] Testing RESET counter = 0"

if ! echo 0 > "$DEVICE"; then
    echo "FAIL: write 0"
    FAIL=$((FAIL + 1))
fi

A=$(cat "$DEVICE")
check_equal "Reset 0" "$A" "0"

# Statistics after reset
echo ""
echo "===== PROC STATS AFTER RESET ====="
cat "$PROC"

# 6. Invalid inputs
echo ""
echo "[6] Testing invalid inputs"

if (echo abc > "$DEVICE") 2>/dev/null; then
    echo "FAIL: accepted abc"
    FAIL=$((FAIL + 1))
else
    echo "PASS: rejected abc"
fi

if (echo -5 > "$DEVICE") 2>/dev/null; then
    echo "FAIL: accepted -5"
    FAIL=$((FAIL + 1))
else
    echo "PASS: rejected -5"
fi

# 7. Final statistics
echo ""
echo "[7] Final procfs statistics"
cat "$PROC" || FAIL=$((FAIL + 1))

# 8. Unload module
echo ""
echo "[8] Unloading counter driver"

if ! rmmod counter_driver; then
    echo "FAIL: rmmod"
    exit 1
fi

# 9. Check cleanup
echo "[9] Checking cleanup"

if grep -q '^counter_driver ' /proc/modules; then
    echo "FAIL: module still loaded"
    FAIL=$((FAIL + 1))
fi

if [ -e "$DEVICE" ] || [ -e "$PROC" ]; then
    echo "FAIL: device or procfs still exists"
    FAIL=$((FAIL + 1))
else
    echo "PASS: cleanup"
fi

# 10. Kernel log
echo ""
echo "[10] Kernel messages"
dmesg | tail -n 30

# Final result
echo ""
echo "================================="
echo " TEST SUMMARY"
echo "================================="

if [ "$FAIL" -eq 0 ]; then
    echo "ALL TASK 2 TESTS PASSED"
    exit 0
else
    echo "FAILED CHECKS: $FAIL"
    exit 1
fi
