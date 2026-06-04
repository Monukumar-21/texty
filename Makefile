CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -Wno-unused-result
TARGET = texty

all: $(TARGET)

$(TARGET): texty.c
	$(CC) $(CFLAGS) -o $(TARGET) texty.c

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET) testfile.txt

.PHONY: all clean run