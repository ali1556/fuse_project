CC = gcc
CFLAGS = -D_FILE_OFFSET_BITS=64 -Wall -Wextra -O2 -g -I.
TRACY_FLAGS = -DTRACY_ENABLE
TRACY_LDFLAGS = -ltracy_client -lpthread -ldl
LDFLAGS = -lfuse3 -lm
TARGET = general_fs
OBJS = main.o fs_operations.o bitmap_manager.o permission_manager.o cli_commands.o

# هدف پیش‌فرض
all: $(TARGET)

# ساخت فایل اجرایی
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(TRACY_LDFLAGS)

# کامپایل با Tracy برای پروفایلینگ
tracy: CFLAGS += $(TRACY_FLAGS)
tracy: $(TARGET)

# کامپایل فایل‌های منبع
main.o: main.c general_fs.h
	$(CC) $(CFLAGS) -c main.c

fs_operations.o: fs_operations.c general_fs.h
	$(CC) $(CFLAGS) -c fs_operations.c

bitmap_manager.o: bitmap_manager.c general_fs.h
	$(CC) $(CFLAGS) -c bitmap_manager.c

permission_manager.o: permission_manager.c general_fs.h
	$(CC) $(CFLAGS) -c permission_manager.c

cli_commands.o: cli_commands.c general_fs.h
	$(CC) $(CFLAGS) -c cli_commands.c

# تست استرس
stress: $(TARGET)
	rm -f stress_test.bin
	./$(TARGET) stress_test.bin /tmp/test_fs -f &
	sleep 2
	fusermount -u /tmp/test_fs
	wait
	./$(TARGET) stress_test.bin /tmp/test_fs stress

# تست استرس با Tracy
stress-tracy: tracy
	rm -f stress_test.bin
	TRACY_NO_EXIT=1 ./$(TARGET) stress_test.bin /tmp/test_fs -f &
	sleep 2
	fusermount -u /tmp/test_fs
	wait
	TRACY_NO_EXIT=1 ./$(TARGET) stress_test.bin /tmp/test_fs stress

# پروفایلینگ با perf
perf-profile: $(TARGET)
	rm -f perf_test.bin
	./$(TARGET) perf_test.bin /tmp/perf_fs -f &
	sleep 2
	perf record -F 99 -a -g -- sleep 30
	fusermount -u /tmp/perf_fs
	wait

# تولید flamegraph
flamegraph:
	wget https://raw.githubusercontent.com/brendangregg/FlameGraph/master/flamegraph.pl
	wget https://raw.githubusercontent.com/brendangregg/FlameGraph/master/stackcollapse-perf.pl
	chmod +x flamegraph.pl stackcollapse-perf.pl
	perf script | ./stackcollapse-perf.pl > out.perf-folded
	./flamegraph.pl out.perf-folded > flamegraph.svg
	echo "Flamegraph generated: flamegraph.svg"

# پاکسازی
clean:
	rm -f $(TARGET) *.o perf_test.bin stress_test.bin out.perf-folded flamegraph.svg

.PHONY: all clean stress stress-tracy perf-profile flamegraph tracy