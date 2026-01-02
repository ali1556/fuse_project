#!/bin/bash

echo "Setting up General FS Test Environment on Ubuntu..."
echo "This script will install all dependencies and configure the project."

# به روزرسانی سیستم
sudo apt update
sudo apt upgrade -y

# نصب پیش‌نیازهای اصلی
echo "Installing build essentials..."
sudo apt install -y build-essential git cmake pkg-config

# نصب FUSE3
echo "Installing FUSE3..."
sudo apt install -y fuse3 libfuse3-dev

# نصب ابزارهای پروفایلینگ
echo "Installing profiling tools..."
sudo apt install -y linux-tools-common linux-tools-generic
sudo apt install -y valgrind
sudo apt install -y python3 python3-pip

# نصب Tracy
echo "Installing Tracy profiler..."
cd ~
if [ ! -d "tracy" ]; then
    git clone https://github.com/wolfpld/tracy.git
fi

cd tracy
mkdir -p build
cd build
cmake ..
make -j$(nproc)
sudo make install

# ساخت profiler GUI
cd ../profiler/build/unix
make -j$(nproc)

# اضافه کردن به مسیر
echo 'export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
echo 'export PATH=$PATH:~/tracy/profiler/build/unix' >> ~/.bashrc
source ~/.bashrc

# نصب FlameGraph
echo "Installing FlameGraph..."
cd ~
git clone https://github.com/brendangregg/FlameGraph.git
echo 'export PATH=$PATH:~/FlameGraph' >> ~/.bashrc
source ~/.bashrc

# ایجاد دایرکتوری‌های تست
mkdir -p ~/test_fs
mkdir -p ~/profiling_results

echo ""
echo "=========================================="
echo "Setup completed successfully!"
echo ""
echo "To build and test the project:"
echo "1. cd /path/to/your/project"
echo "2. chmod +x run_all_tests.sh"
echo "3. ./run_all_tests.sh"
echo ""
echo "To run Tracy profiler:"
echo "  ~/tracy/profiler/build/unix/./Tracy"
echo ""
echo "To generate flamegraph:"
echo "  make flamegraph"
echo "=========================================="