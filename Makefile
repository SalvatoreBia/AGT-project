CC := gcc
# Detect Apple Silicon to fix pkg-config path
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

PKG_CONFIG_ENV :=
ifeq ($(UNAME_S),Darwin)
ifeq ($(UNAME_M),arm64)
# Force Homebrew path for arm64
PKG_CONFIG_ENV := PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig
endif
endif

CFLAGS := -Wall -Wextra -Iinclude -g -O3 $(shell $(PKG_CONFIG_ENV) pkg-config --cflags glib-2.0)
LDFLAGS := $(shell $(PKG_CONFIG_ENV) pkg-config --libs glib-2.0)

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
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Pattern rule for .c → .o mapping in build/
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	./$(TARGET)

test_1000: $(OBJ_DIR)/test_convergence_1000.o $(OBJ_DIR)/src/algorithm.o $(OBJ_DIR)/src/data_structures.o
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/test_convergence_1000 $^ $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR)