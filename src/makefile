BUILD_FLAGS		= -O0 -g -DCHUNKWM_DEBUG -DCHUNKWM_PROFILE -std=c++11 -Wall -Wno-deprecated -Wno-writable-strings
BUILD_PATH		= ../bin
BINS			= $(BUILD_PATH)/float.so
DEV_BIN_PATH	= ../bin
DEV_BUILD_PATH	= ./bin
DEV_BINS		= $(DEV_BUILD_PATH)/float
SRC				= ./plugin.mm
LINK			= -shared -fPIC -framework Carbon -framework Cocoa -framework ApplicationServices
DIR := ${CURDIR}
NOW := $(shell date "+%s")

all: $(BINS)

install: BUILD_FLAGS=-O2 -std=c++11 -Wall -Wno-deprecated -Wno-writable-strings
install: clean $(BINS)
dev: clean $(DEV_BINS)

.PHONY: all clean install dev

$(DEV_BUILD_PATH):
	mkdir -p $(DEV_BUILD_PATH)

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

clean:
	rm -f $(BUILD_PATH)/float.so
	rm -rf $(DEV_BUILD_PATH)/float*
	rm -f $(DEV_BIN_PATH)/float.so

$(DEV_BUILD_PATH)/float: $(SRC) | $(DEV_BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@_$(NOW).so $(LINK)
	ln -sf $(DIR)/$@_$(NOW).so $(DEV_BIN_PATH)/float.so

$(BUILD_PATH)/float.so: $(SRC) | $(BUILD_PATH)
	clang++ $^ $(BUILD_FLAGS) -o $@ $(LINK)
