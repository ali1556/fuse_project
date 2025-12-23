#!/bin/bash

echo "=== Testing User and Permission Features ==="
echo "==========================================="

# پاکسازی
make clean
make

echo -e "\n1. Creating fresh filesystem..."
rm -f user_test.bin
rm -rf /tmp/user_test
mkdir -p /tmp/user_test

echo -e "\n2. Starting filesystem in background..."
./general_fs user_test.bin /tmp/user_test -f &
FS_PID=$!
sleep 3

echo -e "\n3. Testing as root (default)..."
echo "=== Root operations ==="
echo "3.1 Creating files as root..."
echo "root file 1" > /tmp/user_test/root_file1.txt
echo "root file 2" > /tmp/user_test/root_file2.txt
ls -la /tmp/user_test/

echo -e "\n3.2 Changing permissions..."
chmod 600 /tmp/user_test/root_file1.txt
chmod 644 /tmp/user_test/root_file2.txt
ls -la /tmp/user_test/

echo -e "\n3.3 Testing getfacl..."
getfacl /tmp/user_test/root_file1.txt 2>/dev/null || echo "getfacl not available, using stat"
stat /tmp/user_test/root_file1.txt | grep -A1 "Access:"

echo -e "\n4. Creating restricted file..."
echo -e "\n4.1 Creating file with 600 permissions..."
echo "secret content" > /tmp/user_test/secret.txt
chmod 600 /tmp/user_test/secret.txt
echo "File created with permissions 600"

echo -e "\n4.2 Testing access..."
echo "Trying to read as root (should work):"
cat /tmp/user_test/secret.txt && echo "✓ Root can read" || echo "✗ Root cannot read"

echo -e "\n5. Testing directory permissions..."
mkdir /tmp/user_test/restricted_dir
chmod 700 /tmp/user_test/restricted_dir
echo "Restricted directory created"

echo -e "\n6. Testing file ownership operations..."
echo "Note: chown/chgrp require root privileges in real system"
echo "Testing chmod functionality:"
chmod 755 /tmp/user_test/root_file2.txt
ls -la /tmp/user_test/root_file2.txt

echo -e "\n7. Final directory state:"
ls -la /tmp/user_test/

echo -e "\n8. Testing visualization feature..."
fusermount -u /tmp/user_test
wait $FS_PID

echo -e "\n9. Running final visualization..."
./general_fs user_test.bin /tmp/user_test -f &
FS_PID=$!
sleep 2
kill $FS_PID
wait $FS_PID

echo -e "\n10. Cleanup..."
rm -f user_test.bin
rm -rf /tmp/user_test

echo -e "\n✅ User and permission tests completed!"