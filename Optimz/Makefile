CXX ?= clang++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic
LDFLAGS ?=

SRC_DIR := src
BIN_DIR := bin
OBJ_DIR := build

TARGET := $(BIN_DIR)/Opt

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

.PHONY: all clean samples test install

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

samples: all
	$(MAKE) -C samples

test: samples all
	@echo "Running sample optimization..."
	./$(TARGET) samples/hello -2 | cat

install: all
	install -m 0755 $(TARGET) /usr/local/bin/Opt

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	$(MAKE) -C samples clean || true

