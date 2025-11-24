#!/bin/bash

echo "=== FUSE File System Manual Test ==="

# Start the file system in background
cd ~/fuse_project
echo "Starting FUSE file system..."
./general_fs my_disk.bin ~/my_mount -f &
FS_PID=$!
echo "File system started with PID: $FS_PID"

# Wait for file system to fully start
sleep 3

# Change to mount point and run tests
cd ~/my_mount

echo ""
echo "=== Basic File Operations ==="

# Test 1: Create file (open with CREATE)
echo "Test 1: Creating file1.txt"
echo "Hello FUSE File System" > file1.txt

# Test 2: Read file
echo "Test 2: Reading file1.txt"
echo "Content:"
cat file1.txt

# Test 3: Append to file (write)
echo "Test 3: Appending to file1.txt"
echo "This is additional content" >> file1.txt
echo "Updated content:"
cat file1.txt

# Test 4: Create multiple files
echo "Test 4: Creating multiple files"
echo "File 2 content" > file2.txt
echo "File 3 content" > file3.txt

echo ""
echo "=== File Listing ==="
echo "Current directory contents:"
ls -la

echo ""
echo "=== File Shrink Test ==="
# Create a larger file
echo "Test 5: Creating large file for shrink test"
for i in {1..20}; do
    echo "This is line number $i" >> large_file.txt
done
echo "Original file size:"
ls -l large_file.txt | awk '{print $5 " bytes"}'

# Shrink the file
echo "Shrinking file to 50 bytes..."
truncate -s 50 large_file.txt
echo "New file size:"
ls -l large_file.txt | awk '{print $5 " bytes"}'
echo "Content after shrink:"
cat large_file.txt

echo ""
echo "=== File Statistics ==="
echo "Test 6: File stats for file1.txt"
stat file1.txt

echo ""
echo "=== System Statistics ==="
echo "Test 7: File system usage"
df -h ~/my_mount | grep my_mount

echo ""
echo "=== File Deletion ==="
echo "Test 8: Deleting test files"
rm file1.txt file2.txt file3.txt large_file.txt
echo "Files after deletion:"
ls -la

echo ""
echo "=== Advanced Tests ==="
echo "Test 9: Binary file test"
cp /bin/ls ./copied_ls
chmod +x ./copied_ls
echo "Binary file copied and made executable"
ls -l copied_ls

echo "Test 10: Directory operations"
mkdir test_dir
echo "Directory created"
ls -la | grep test_dir

echo ""
echo "=== Final System Status ==="
echo "Total files in system:"
ls -1 | wc -l
echo "Disk usage:"
df -h ~/my_mount | grep my_mount

echo ""
echo "=== Cleaning up ==="
rm -f copied_ls
rmdir test_dir

# Stop the file system
echo "Stopping FUSE file system..."
cd ~/fuse_project
kill $FS_PID
wait $FS_PID 2>/dev/null

echo "=== Test completed successfully ==="