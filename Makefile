CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -D_FILE_OFFSET_BITS=64 -g
LIBS = -lfuse3
TARGET = general_fs
SOURCES = main.c fs_operations.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -f $(TARGET) my_disk.bin

test: $(TARGET)
	mkdir -p mount_point
	./$(TARGET) my_disk.bin mount_point -f

.PHONY: all clean test