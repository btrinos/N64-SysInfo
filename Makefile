BUILD_DIR = build
SOURCE_DIR = src
ASSETS_DIR = assets

N64_ROM_TITLE = "N64 SysInfo"

ifneq ($(filter test tests/get_cpu_revision_test,$(MAKECMDGOALS)),)
SKIP_N64 := 1
endif

ifeq ($(SKIP_N64),)
include $(N64_INST)/include/n64.mk
endif
		
		# Source files
		OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/cpu_revision.o
		
		HOST_CC ?= gcc
		HOST_CFLAGS ?= -std=c99 -Wall -Wextra -pedantic
		
# Build the ROM
$(BUILD_DIR)/n64-sysinfo.z64: N64_ROM_TITLE = "N64 SysInfo"

all: n64-sysinfo.z64

$(BUILD_DIR)/main.o: $(SOURCE_DIR)/main.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cpu_revision.o: $(SOURCE_DIR)/cpu_revision.c $(SOURCE_DIR)/cpu_revision.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SOURCE_DIR)/cpu_revision.c -o $@

n64-sysinfo.z64: $(OBJS)
	$(N64_CC) $(N64_LDFLAGS) -o n64-sysinfo.elf $(OBJS) $(N64_LIBS)
	$(N64_OBJCOPY) n64-sysinfo.elf n64-sysinfo.z64 -O binary

clean:
	rm -rf $(BUILD_DIR) n64-sysinfo.elf n64-sysinfo.z64
	rm -f tests/get_cpu_revision_test

test: tests/get_cpu_revision_test
	./tests/get_cpu_revision_test

tests/get_cpu_revision_test: tests/get_cpu_revision_test.c $(SOURCE_DIR)/cpu_revision.c $(SOURCE_DIR)/cpu_revision.h
	$(HOST_CC) $(HOST_CFLAGS) -I$(SOURCE_DIR) $< $(SOURCE_DIR)/cpu_revision.c -o $@

.PHONY: all clean
