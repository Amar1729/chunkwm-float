export PATH := /usr/local/bin:$(PATH)
BUILD_FLAGS		= -O0 -g -std=c++11 -Wall
BUILD_PATH		= ./bin
SRC				= ./src/plugin.mm
BINS			= $(BUILD_PATH)/float.so
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices

all: $(BINS)

install: BUILD_FLAGS=-O2 -Wall
install: clean $(BINS)

.PHONY: all clean install format

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -f $(BUILD_PATH)/float.so

$(BUILD_PATH)/float.so: $(SRC) | $(BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@ $(LINK)

format:
	clang-format -i -style=file $(SRC)
