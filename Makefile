.PHONY: all clean run debug release

CC := clang

TARGET := sim
BUILD_DIR := build

SOURCES := main.c
OBJECTS := $(SOURCES:%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJECTS:.o=.d)

CPPFLAGS :=
CFLAGS := -std=c23 -Wall -Wextra -Wpedantic -Werror
LDFLAGS :=
LDLIBS := -lm

all: release

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

debug: CFLAGS += -O0 -g3 -DDEBUG
debug: clean $(TARGET)

release: CFLAGS += -O2 -DNDEBUG
release: clean $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
