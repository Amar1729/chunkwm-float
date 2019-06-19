BUILD_PATH = ./plugins
BIN = $(BUILD_PATH)/float.so
LIB = ./target/release/libfloat.dylib

install: clean $(BUILD_PATH)
	cargo build --release
	cp $(LIB) $(BIN)

clean:
	rm -f $(BIN)

$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)
