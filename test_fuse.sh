#!/bin/bash

echo "=== شروع تست سیستم فایل FUSE ==="

# 1. به پوشه پروژه برو
cd ~/fuse_project

echo "1. در حال اجرای سیستم فایل در background..."
./general_fs my_disk.bin ~/my_mount -f &
FS_PID=$!
echo "   Process ID: $FS_PID"

# صبر کن سیستم فایل کامل بالا بیاد
sleep 2

# 2. به پوشه mount شده برو
cd ~/my_mount

echo "2. تست ایجاد فایل..."
echo "این اولین تست منه" > test1.txt
echo "   فایل test1.txt ایجاد شد"

echo "3. تست خواندن فایل..."
echo "   محتوای test1.txt:"
cat test1.txt

echo "4. تست نوشتن اضافی..."
echo "خط دوم" >> test1.txt
echo "   محتوای بعد از append:"
cat test1.txt

echo "5. تست ایجاد فایل‌های بیشتر..."
echo "فایل تست دوم" > file2.txt
echo "فایل تست سوم" > file3.txt

echo "6. تست لیست فایل‌ها..."
ls -la

echo "7. تست shrink (کاهش سایز فایل)..."
# ایجاد فایل بزرگ
for i in {1..50}; do
    echo "این خط شماره $i است" >> large_file.txt
done
echo "   سایز اولیه large_file.txt:"
ls -l large_file.txt | awk '{print $5 " bytes"}'

# کاهش سایز
truncate -s 100 large_file.txt
echo "   سایز بعد از shrink:"
ls -l large_file.txt | awk '{print $5 " bytes"}'
echo "   محتوای بعد از shrink:"
cat large_file.txt

echo "8. تست آمار فایل (get_file_stats)..."
echo "   آمار test1.txt:"
stat test1.txt

echo "9. تست آمار سیستم (get_stats)..."
echo "   فضای استفاده شده:"
df -h ~/my_mount | grep my_mount

echo "10. تست حذف فایل..."
rm test1.txt file2.txt file3.txt large_file.txt
echo "   فایل‌ها حذف شدند"
echo "   لیست نهایی:"
ls -la

echo "11. متوقف کردن سیستم فایل..."
# برگرد به پوشه پروژه
cd ~/fuse_project

# سیستم فایل رو متوقف کن
kill $FS_PID
wait $FS_PID 2>/dev/null

echo "=== تست کامل شد ==="