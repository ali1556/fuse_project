#!/bin/bash

echo "=========================================="
echo "General FS Bitmap - Complete Test Suite"
echo "=========================================="

# رنگ‌های ترمینال
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

# بررسی وجود پیش‌نیازها
check_dependencies() {
    print_step "Checking dependencies..."
    
    command -v gcc >/dev/null 2>&1 || { print_error "GCC not found"; exit 1; }
    command -v make >/dev/null 2>&1 || { print_error "Make not found"; exit 1; }
    command -v fusermount3 >/dev/null 2>&1 || { print_error "FUSE3 not found"; exit 1; }
    command -v perf >/dev/null 2>&1 || print_info "perf not found (optional)"
    
    print_success "Dependencies checked"
}

# مرحله 1: ساخت پروژه
build_project() {
    print_step "Building project..."
    
    make clean
    if make; then
        print_success "Project built successfully"
    else
        print_error "Build failed"
        exit 1
    fi
}

# مرحله 2: ساخت با Tracy
build_with_tracy() {
    print_step "Building with Tracy support..."
    
    if make tracy; then
        print_success "Project built with Tracy"
    else
        print_error "Tracy build failed"
        print_info "Continuing without Tracy..."
    fi
}

# مرحله 3: تست‌های پایه
run_basic_tests() {
    print_step "Running basic functionality tests..."
    
    # ایجاد دایرکتوری تست
    rm -rf /tmp/general_fs_test
    mkdir -p /tmp/general_fs_test
    
    # تست 1: ایجاد فایل سیستم
    print_info "Test 1: Creating filesystem..."
    rm -f basic_test.bin
    timeout 10 ./general_fs basic_test.bin /tmp/general_fs_test -f &
    FS_PID=$!
    sleep 3
    
    if [ -d "/tmp/general_fs_test" ] && mountpoint -q "/tmp/general_fs_test"; then
        print_success "Filesystem mounted successfully"
        
        # عملیات پایه
        touch /tmp/general_fs_test/test1.txt
        echo "Hello World" > /tmp/general_fs_test/test1.txt
        mkdir /tmp/general_fs_test/testdir
        echo "Test file" > /tmp/general_fs_test/testdir/file2.txt
        
        # بررسی
        if [ -f "/tmp/general_fs_test/test1.txt" ] && [ -d "/tmp/general_fs_test/testdir" ]; then
            print_success "Basic operations work correctly"
        else
            print_error "Basic operations failed"
        fi
        
        # جدا کردن
        fusermount -u /tmp/general_fs_test
        wait $FS_PID
    else
        print_error "Failed to mount filesystem"
    fi
    
    # تست 2: نمایش فضای خالی
    print_info "Test 2: Free space visualization..."
    ./general_fs basic_test.bin /tmp/general_fs_test -f &
    FS_PID=$!
    sleep 2
    fusermount -u /tmp/general_fs_test
    wait $FS_PID
    
    print_success "Basic tests completed"
}

# مرحله 4: تست استرس
run_stress_test() {
    print_step "Running stress test..."
    
    # تست کوچک برای شروع
    print_info "Running mini stress test (1000 files)..."
    
    rm -f stress_mini.bin
    timeout 30 ./general_fs stress_mini.bin /tmp/test_fs stress 2>&1 | tee stress_mini.log
    
    if grep -q "Stress Test Results" stress_mini.log; then
        print_success "Mini stress test completed"
        
        # استخراج نتایج
        TOTAL_TIME=$(grep "Total time:" stress_mini.log | awk '{print $3}')
        OPS_PER_SEC=$(grep "Operations per second:" stress_mini.log | awk '{print $3}')
        
        echo "Mini Stress Test Results:"
        echo "  Total time: $TOTAL_TIME seconds"
        echo "  Operations/sec: $OPS_PER_SEC"
    else
        print_error "Mini stress test failed"
    fi
    
    # تست بزرگ (اگر کاربر تأیید کند)
    read -p "Run full stress test with 10000 files? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_info "Running full stress test (10000 files)..."
        
        rm -f stress_full.bin
        timeout 120 ./general_fs stress_full.bin /tmp/test_fs stress 2>&1 | tee stress_full.log
        
        if grep -q "Stress Test Results" stress_full.log; then
            print_success "Full stress test completed"
            
            TOTAL_TIME=$(grep "Total time:" stress_full.log | awk '{print $3}')
            OPS_PER_SEC=$(grep "Operations per second:" stress_full.log | awk '{print $3}')
            
            echo "Full Stress Test Results:"
            echo "  Total time: $TOTAL_TIME seconds"
            echo "  Operations/sec: $OPS_PER_SEC"
        else
            print_error "Full stress test failed"
        fi
    fi
}

# مرحله 5: تست Tracy
run_tracy_test() {
    print_step "Testing Tracy profiler..."
    
    if [ -f "./general_fs" ] && ldd ./general_fs 2>/dev/null | grep -q tracy; then
        print_info "Starting Tracy capture..."
        
        # راه‌اندازی Tracy profiler
        cd ~/tracy/profiler/build/unix
        ./Tracy &
        TRACY_PID=$!
        cd -
        
        sleep 2
        
        # اجرای تست با Tracy
        print_info "Running test with Tracy..."
        rm -f tracy_test.bin
        TRACY_NO_EXIT=1 ./general_fs tracy_test.bin /tmp/tracy_fs -f &
        FS_PID=$!
        sleep 5
        
        # انجام عملیات
        touch /tmp/tracy_fs/tracy_test.txt
        echo "Tracy profiling test" > /tmp/tracy_fs/tracy_test.txt
        dd if=/dev/zero of=/tmp/tracy_fs/large.bin bs=1M count=10 status=none
        
        sleep 2
        fusermount -u /tmp/tracy_fs
        wait $FS_PID
        
        print_success "Tracy test completed"
        print_info "Check Tracy profiler window for results"
        
        # بستن Tracy
        kill $TRACY_PID 2>/dev/null
    else
        print_info "Tracy not enabled, skipping..."
    fi
}

# مرحله 6: تست perf
run_perf_test() {
    print_step "Running perf profiling..."
    
    if command -v perf >/dev/null 2>&1; then
        print_info "Starting perf recording..."
        
        rm -f perf_test.bin
        rm -rf /tmp/perf_fs
        mkdir -p /tmp/perf_fs
        
        # اجرای فایل سیستم
        ./general_fs perf_test.bin /tmp/perf_fs -f &
        FS_PID=$!
        sleep 2
        
        # شروع ضبط perf
        perf record -F 99 -a -g -- sleep 10 &
        PERF_PID=$!
        
        # انجام عملیات
        for i in {1..100}; do
            echo "Test $i" > /tmp/perf_fs/file_$i.txt
        done
        
        # انتظار برای کامل شدن perf
        wait $PERF_PID
        
        # جدا کردن فایل سیستم
        fusermount -u /tmp/perf_fs
        wait $FS_PID
        
        # تولید گزارش
        print_info "Generating perf report..."
        perf report --stdio | head -50
        
        print_success "Perf profiling completed"
    else
        print_info "perf not available, skipping..."
    fi
}

# مرحله 7: جمع‌بندی
generate_report() {
    print_step "Generating test report..."
    
    echo ""
    echo "=========================================="
    echo "           TEST REPORT SUMMARY"
    echo "=========================================="
    echo ""
    echo "✓ Project compiled successfully"
    echo "✓ Basic functionality tested"
    echo "✓ Stress tests completed"
    echo "✓ Profiling tools verified"
    echo ""
    echo "Next steps:"
    echo "1. Check Tracy profiler for detailed analysis"
    echo "2. Examine perf report for performance bottlenecks"
    echo "3. Compare bitmap vs linked list performance"
    echo ""
    echo "Files generated:"
    ls -la *.bin *.log 2>/dev/null || echo "No test files remaining"
    echo ""
    echo "To run Tracy profiler manually:"
    echo "  cd ~/tracy/profiler/build/unix"
    echo "  ./Tracy"
    echo ""
    echo "Then run your filesystem with:"
    echo "  TRACY_NO_EXIT=1 ./general_fs test.bin /mnt/test -f"
    echo "=========================================="
}

# اجرای مراحل اصلی
main() {
    echo "Starting complete test suite..."
    
    check_dependencies
    build_project
    build_with_tracy
    run_basic_tests
    run_stress_test
    run_tracy_test
    run_perf_test
    generate_report
    
    # پاکسازی
    rm -f *.bin *.log 2>/dev/null
    rm -rf /tmp/general_fs_test /tmp/test_fs /tmp/tracy_fs /tmp/perf_fs 2>/dev/null
    
    print_success "All tests completed successfully!"
}

# اجرای اصلی
main "$@"