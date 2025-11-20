CC := gcc
CFLAGS := -Wall -Wextra -Iinclude -g -O3

SRC := main.c $(wildcard src/*.c)
OBJ_DIR := build
OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))
TARGET := $(OBJ_DIR)/main

.PHONY: all run clean dirs

all: dirs $(TARGET)

dirs:
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)/src

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Pattern rule for .c → .o mapping in build/
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR)