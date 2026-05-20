APP_NAME := HardwareProtocolFuzzerDemo
BUILD_DIR := build
SOURCE := Sources/App.swift

.PHONY: all run clean

all: $(BUILD_DIR)/$(APP_NAME)

$(BUILD_DIR)/$(APP_NAME): $(SOURCE)
	mkdir -p $(BUILD_DIR)
	swiftc $(SOURCE) -framework Cocoa -o $(BUILD_DIR)/$(APP_NAME)

run: all
	./$(BUILD_DIR)/$(APP_NAME)

clean:
	rm -rf $(BUILD_DIR)
