# Makefile for cse4001_sync

CC = gcc
CFLAGS = -std=c11 -O2 -Wall -pthread
TARGET = cse4001_sync
SRC = cse4001_sync.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
