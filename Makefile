CC := gcc
CFLAGS := -Wall -Wextra -Iinclude -g

SRC := main.c $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
TARGET := main

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

