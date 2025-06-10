CC = gcc

CFLAGS = -Wall -Wextra -O2

TARGET = ext2shell

SRCS = main.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
