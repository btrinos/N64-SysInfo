BUILD_DIR = build
SOURCE_DIR = src
ASSETS_DIR = assets

N64_ROM_TITLE = "N64 SysInfo"

# Skip N64 toolchain for host tests
ifneq ($(filter test tests/get_cpu_revision_test,$(MAKECMDGOALS)),)
SKIP_N64 := 1
endif

ifeq ($(SKIP_N64),)
include $(N64_INST)/include/n64.mk
endif

# Source files
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/cpu_revision.o

# Host compiler for tests
HOST_CC ?= gcc
HOST_CFLAGS ?= -std=c99 -Wall -Wextra -pedantic

# Default target
all: n64-sysinfo.z64

# Build object files
$(BUILD_DIR)/main.o: $(SOURCE_DIR)/main.c $(SOURCE_DIR)/cpu_revision.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cpu_revision.o: $(SOURCE_DIR)/cpu_revision.c $(SOURCE_DIR)/cpu_revision.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link and create ROM
n64-sysinfo.z64: $(OBJS)
	@echo "Linking N64 ROM..."
	$(LD) -o $(BUILD_DIR)/n64-sysinfo.elf $(OBJS) $(LDFLAGS) $(N64_LIBS)
	@rm -f $@
	$(N64TOOL) $(N64_FLAGS) -o $@ $(BUILD_DIR)/n64-sysinfo.elf
	$(CHKSUM64) $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) n64-sysinfo.z64
	rm -f tests/get_cpu_revision_test

# Host unit tests
test: tests/get_cpu_revision_test
	@echo "Running CPU revision tests..."
	./tests/get_cpu_revision_test
	@echo "All tests passed!"

tests/get_cpu_revision_test: tests/get_cpu_revision_test.c $(SOURCE_DIR)/cpu_revision.c $(SOURCE_DIR)/cpu_revision.h
	@mkdir -p tests
	$(HOST_CC) $(HOST_CFLAGS) -I$(SOURCE_DIR) $< $(SOURCE_DIR)/cpu_revision.c -o $@

.PHONY: all clean test
