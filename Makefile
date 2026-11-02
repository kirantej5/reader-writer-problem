CC ?= gcc
CFLAGS ?= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Iinclude
LDLIBS ?= -pthread

BUILD_DIR := build
COMMON_OBJECT := $(BUILD_DIR)/rwlock.o

.PHONY: all test clean

all: $(BUILD_DIR)/reader_writer

test: $(BUILD_DIR)/test_rwlock
	./$(BUILD_DIR)/test_rwlock

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/rwlock.o: src/rwlock.c include/rwlock.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/reader_writer: src/main.c $(COMMON_OBJECT) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/test_rwlock: tests/test_rwlock.c $(COMMON_OBJECT) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -rf $(BUILD_DIR)
