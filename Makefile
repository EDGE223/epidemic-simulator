.PHONY: all clean run debug release

CC := clang
CXX := clang++

TARGET := sim

BUILD_DIR := build
MODE ?= release
OUT_DIR := $(BUILD_DIR)/$(MODE)

C_SOURCES := simulator.c
CXX_SOURCES := main.cpp

C_OBJECTS := $(patsubst %.c,$(OUT_DIR)/%.o,$(C_SOURCES))
CXX_OBJECTS := $(patsubst %.cpp,$(OUT_DIR)/%.o,$(CXX_SOURCES))

OBJECTS := $(C_OBJECTS) $(CXX_OBJECTS)
DEPS := $(OBJECTS:.o=.d)

CPPFLAGS := -Iinclude

CFLAGS := -std=c23 -Wall -Wextra -Wpedantic -Werror
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -Werror

ifeq ($(MODE),debug)
    CFLAGS += -O0 -g3 -DDEBUG
    CXXFLAGS += -O0 -g3 -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
    CXXFLAGS += -O2 -DNDEBUG
endif

LDFLAGS :=
LDLIBS := -lm

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OUT_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUT_DIR)/%.o: %.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

debug:
	$(MAKE) MODE=debug all

release:
	$(MAKE) MODE=release all

run: all
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
