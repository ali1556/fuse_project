CC = gcc
CFLAGS = -Wall -Wextra -D_FILE_OFFSET_BITS=64 -g
LIBS = -lfuse3
TARGET = general_fs
OBJS = main.o fs_operations.o free_list.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

main.o: main.c general_fs.h
	$(CC) $(CFLAGS) -c main.c

fs_operations.o: fs_operations.c general_fs.h
	$(CC) $(CFLAGS) -c fs_operations.c

free_list.o: free_list.c general_fs.h
	$(CC) $(CFLAGS) -c free_list.c

clean:
	rm -f $(TARGET) $(OBJS)

test: $(TARGET)
	@echo "Creating test directory..."
	mkdir -p /tmp/test_fs
	@echo "Running filesystem..."
	./$(TARGET) test_disk.bin /tmp/test_fs -f

viz: $(TARGET)
	@echo "Visualizing free space..."
	./$(TARGET) test_disk.bin /tmp/test_fs -f &
	sleep 2
	fusermount -u /tmp/test_fs

.PHONY: all clean test viz