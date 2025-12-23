CC = gcc
CFLAGS = -Wall -Wextra -D_FILE_OFFSET_BITS=64 -g -DFUSE_USE_VERSION=31
LIBS = -lfuse3
TARGET = general_fs
OBJS = main.o fs_operations.o free_list.o user_manager.o permission_manager.o cli_commands.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

main.o: main.c general_fs.h
	$(CC) $(CFLAGS) -c main.c

fs_operations.o: fs_operations.c general_fs.h
	$(CC) $(CFLAGS) -c fs_operations.c

free_list.o: free_list.c general_fs.h
	$(CC) $(CFLAGS) -c free_list.c

user_manager.o: user_manager.c general_fs.h
	$(CC) $(CFLAGS) -c user_manager.c

permission_manager.o: permission_manager.c general_fs.h
	$(CC) $(CFLAGS) -c permission_manager.c

cli_commands.o: cli_commands.c general_fs.h
	$(CC) $(CFLAGS) -c cli_commands.c

clean:
	rm -f $(TARGET) $(OBJS) *.bin *.log
	rm -rf /tmp/fuse_* /tmp/test_fs /tmp/my_fs

test: $(TARGET)
	@echo "Running quick test..."
	@mkdir -p /tmp/test_fs
	@./$(TARGET) test.bin /tmp/test_fs -f &
	@sleep 2
	@echo "test" > /tmp/test_fs/test.txt 2>/dev/null && echo "✓ Write test passed" || echo "✗ Write test failed"
	@cat /tmp/test_fs/test.txt 2>/dev/null | grep -q "test" && echo "✓ Read test passed" || echo "✗ Read test failed"
	@fusermount -u /tmp/test_fs 2>/dev/null || true

.PHONY: all clean test