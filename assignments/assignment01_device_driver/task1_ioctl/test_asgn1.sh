
#!/bin/sh

MODULE="/asgn1_driver.ko"
DEVICE="/dev/asgn1"
TEST_APP="/test_ioctl_arm"

echo "================================="
echo " ASSIGNMENT 01 - TASK 1 TEST"
echo "================================="

if grep -q '^asgn1_driver ' /proc/modules; then
    echo "ERROR: Module already loaded"
    exit 1
fi

echo "[1] Loading kernel module"
insmod "$MODULE" || exit 1

echo "[2] Checking device node"
if [ ! -c "$DEVICE" ]; then
    echo "Device node not found, creating it"
    mknod "$DEVICE" c 241 0 || exit 1
fi

ls -l "$DEVICE"

echo "[3] Running ioctl tests"
"$TEST_APP"
RESULT=$?

echo "[4] Driver statistics"
cat /proc/asgn1_info

echo "[5] Unloading module"
rmmod asgn1_driver || exit 1

echo "[6] Checking cleanup"

if grep -q '^asgn1_driver ' /proc/modules; then
    echo "FAIL: Module still loaded"
    exit 1
fi

if [ -e "$DEVICE" ]; then
    echo "FAIL: Device still exists"
    exit 1
fi

if [ -e /proc/asgn1_info ]; then
    echo "FAIL: Procfs entry still exists"
    exit 1
fi

echo "Cleanup PASS"

echo "[7] Kernel messages"
dmesg | tail -n 40

if [ "$RESULT" -ne 0 ]; then
    echo "IOCTL TEST FAILED"
    exit 1
fi

echo "================================="
echo " ALL TASK 1 TESTS PASSED"
echo "================================="

exit 0
