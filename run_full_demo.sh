#!/bin/bash

echo "Full General FS Demo"
echo "======================"

# پاکسازی
make clean
make

echo -e "\n📁 Creating fresh filesystem..."
rm -f demo.bin
rm -rf /tmp/demo_fs
mkdir -p /tmp/demo_fs

echo -e "\n🚀 Starting filesystem..."
./general_fs demo.bin /tmp/demo_fs -f &
FS_PID=$!
sleep 3

echo -e "\n✅ Testing Basic Operations:"
echo "----------------------------"
echo "1. File creation..."
touch /tmp/demo_fs/file1.txt
echo "2. Writing..."
echo "Hello World!" > /tmp/demo_fs/file1.txt
echo "3. Reading..."
cat /tmp/demo_fs/file1.txt
echo "4. Directory creation..."
mkdir /tmp/demo_fs/docs
echo "5. Multiple files..."
for i in {1..3}; do
    echo "File $i" > /tmp/demo_fs/file_$i.txt
done

echo -e "\n🧠 Testing Free List Management:"
echo "--------------------------------"
echo "6. Large file allocation..."
dd if=/dev/zero of=/tmp/demo_fs/large.bin bs=500K count=1 status=none
echo "7. File deletion (freeing blocks)..."
rm /tmp/demo_fs/file_2.txt
echo "8. New file in freed space..."
dd if=/dev/zero of=/tmp/demo_fs/new.bin bs=300K count=1 status=none

echo -e "\n📊 Final State:"
echo "--------------"
ls -lh /tmp/demo_fs/
echo -e "\nTotal files: $(find /tmp/demo_fs -type f | wc -l)"

echo -e "\n👁️  Testing Visualization:"
echo "--------------------------"
echo "Unmounting for visualization..."
fusermount -u /tmp/demo_fs
wait $FS_PID

echo -e "\n🖥️  Visualization Output:"
echo "========================"
./general_fs demo.bin /tmp/demo_fs -f &
FS_PID=$!
sleep 2
kill $FS_PID
wait $FS_PID

echo -e "\n🧹 Cleaning up..."
rm -f demo.bin
rm -rf /tmp/demo_fs

echo -e "\n🎉 Demo completed successfully!"