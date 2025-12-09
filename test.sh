#!/bin/bash

echo "=== General FS Test Script ==="
echo ""

# کامپایل
echo "1. Compiling..."
make clean
make

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "Compilation successful!"
echo ""

# پاکسازی تست‌های قبلی
echo "2. Cleaning previous tests..."
rm -f test_disk.bin
rm -rf /tmp/test_fs
mkdir -p /tmp/test_fs

echo ""

# ایجاد و مانت فایل سیستم
echo "3. Creating and mounting filesystem..."
./general_fs test_disk.bin /tmp/test_fs -f &
FS_PID=$!

# صبر کردن برای مانت شدن
sleep 2

echo "Filesystem mounted at /tmp/test_fs"
echo ""

# اجرای دستورات تست
echo "4. Running test commands..."

echo "4.1 Creating directories..."
mkdir /tmp/test_fs/dir1
mkdir /tmp/test_fs/dir2

echo "4.2 Creating files..."
echo "Hello World" > /tmp/test_fs/file1.txt
echo "This is a test" > /tmp/test_fs/file2.txt

echo "4.3 Writing to files..."
echo "Additional content" >> /tmp/test_fs/file1.txt
echo "More test data" > /tmp/test_fs/file3.txt

echo "4.4 Listing files..."
ls -la /tmp/test_fs/

echo "4.5 Reading files..."
cat /tmp/test_fs/file1.txt
echo "---"
cat /tmp/test_fs/file2.txt

echo "4.6 Creating large file..."
dd if=/dev/urandom of=/tmp/test_fs/large.bin bs=1M count=5 status=none
echo "Large file created"

echo "4.7 Deleting files..."
rm /tmp/test_fs/file2.txt
rm /tmp/test_fs/file3.txt

echo "4.8 Final listing..."
ls -la /tmp/test_fs/

echo ""

# آنمانت
echo "5. Unmounting filesystem..."
fusermount -u /tmp/test_fs
wait $FS_PID

echo ""

# نمایش وضعیت نهایی
echo "6. Final visualization..."
./general_fs test_disk.bin /tmp/test_fs -f &
FS_PID=$!
sleep 2
fusermount -u /tmp/test_fs
wait $FS_PID

echo ""
echo "=== Test completed ==="