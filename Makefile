BUILD_DIR  := build
BUILD_TYPE ?= Debug
ARGS       ?=

.PHONY: all configure build clean test release compdb run help

all: build

configure:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR) --parallel

clean:
	rm -rf $(BUILD_DIR)

test:
	cmake -B $(BUILD_DIR) \
	      -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	      -DBUILD_TESTS=ON
	cmake --build $(BUILD_DIR) --parallel
	ctest --test-dir $(BUILD_DIR) --output-on-failure --parallel

release:
	$(MAKE) BUILD_TYPE=Release build

compdb: configure
	ln -sf $(BUILD_DIR)/compile_commands.json compile_commands.json

run: build
	$(BUILD_DIR)/wavelet_analysis $(ARGS)

help:
	@printf "Targets:\n"
	@printf "  build    Configure and compile in Debug mode (default)\n"
	@printf "  release  Build in Release mode\n"
	@printf "  test     Build with tests enabled and run them\n"
	@printf "  clean    Remove build directory\n"
	@printf "  compdb   Symlink compile_commands.json for clangd\n"
	@printf "  run      Run the binary (pass ARGS= for arguments)\n"
