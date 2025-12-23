#!/bin/bash

echo "=== Testing File Operations with Permissions ==="
echo "================================================"

make clean
make

echo -e "\n1. Setting up test environment..."
rm -f perm_ops.bin
rm -rf /tmp/perm_ops
mkdir -p /tmp/perm_ops

echo -e "\n2. Starting filesystem..."
./general_fs perm_ops.bin /tmp/perm_ops -f &
FS_PID=$!
sleep 3

echo -e "\n3. Creating test files with different permissions..."
echo "=== Creating files ==="

# فایل با دسترسی کامل
echo "public file" > /tmp/perm_ops/public.txt
chmod 777 /tmp/perm_ops/public.txt
echo "Created: public.txt (777)"

# فایل فقط خواندنی
echo "read only" > /tmp/perm_ops/readonly.txt
chmod 444 /tmp/perm_ops/readonly.txt
echo "Created: readonly.txt (444)"

# فایل فقط مالک
echo "owner only" > /tmp/perm_ops/owner.txt
chmod 600 /tmp/perm_ops/owner.txt
echo "Created: owner.txt (600)"

# دایرکتوری با دسترسی محدود
mkdir /tmp/perm_ops/private_dir
chmod 700 /tmp/perm_ops/private_dir
echo "Created: private_dir (700)"

echo -e "\n4. Testing read operations..."
echo "=== Read tests ==="
cat /tmp/perm_ops/public.txt && echo "✓ Can read public.txt" || echo "✗ Cannot read public.txt"
cat /tmp/perm_ops/readonly.txt && echo "✓ Can read readonly.txt" || echo "✗ Cannot read readonly.txt"

echo -e "\n5. Testing write operations..."
echo "=== Write tests ==="
echo "adding text" >> /tmp/perm_ops/public.txt 2>/dev/null && echo "✓ Can write to public.txt" || echo "✗ Cannot write to public.txt"
echo "adding text" >> /tmp/perm_ops/readonly.txt 2>/dev/null && echo "✓ Can write to readonly.txt" || echo "✗ Cannot write to readonly.txt"

echo -e "\n6. Testing directory operations..."
echo "=== Directory tests ==="
ls /tmp/perm_ops/private_dir 2>/dev/null && echo "✓ Can list private_dir" || echo "✗ Cannot list private_dir"
touch /tmp/perm_ops/private_dir/test.txt 2>/dev/null && echo "✓ Can create in private_dir" || echo "✗ Cannot create in private_dir"

echo -e "\n7. Testing permission changes..."
echo "=== Permission change tests ==="
chmod 755 /tmp/perm_ops/readonly.txt 2>/dev/null && echo "✓ Can change permissions" || echo "✗ Cannot change permissions"
ls -la /tmp/perm_ops/readonly.txt

echo -e "\n8. Testing file deletion..."
echo "=== Deletion tests ==="
rm /tmp/perm_ops/public.txt 2>/dev/null && echo "✓ Can delete public.txt" || echo "✗ Cannot delete public.txt"
rm /tmp/perm_ops/owner.txt 2>/dev/null && echo "✓ Can delete owner.txt" || echo "✗ Cannot delete owner.txt"

echo -e "\n9. Final directory state:"
ls -la /tmp/perm_ops/

echo -e "\n10. Unmounting and cleanup..."
fusermount -u /tmp/perm_ops
wait $FS_PID

rm -f perm_ops.bin
rm -rf /tmp/perm_ops

echo -e "\n✅ File operation tests completed!"