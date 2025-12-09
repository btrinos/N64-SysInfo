BUILD_DIR = build
SOURCE_DIR = src
ASSETS_DIR = assets

N64_ROM_TITLE = "N64 SysInfo"

include $(N64_INST)/include/n64.mk

# Source files
OBJS = $(BUILD_DIR)/main.o

# Build the ROM
$(BUILD_DIR)/n64-sysinfo.z64: N64_ROM_TITLE = "N64 SysInfo"

all: n64-sysinfo.z64

$(BUILD_DIR)/main.o: $(SOURCE_DIR)/main.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

n64-sysinfo.z64: $(OBJS)
	$(N64_CC) $(N64_LDFLAGS) -o n64-sysinfo.elf $(OBJS) $(N64_LIBS)
	$(N64_OBJCOPY) n64-sysinfo.elf n64-sysinfo.z64 -O binary

clean:
	rm -rf $(BUILD_DIR) n64-sysinfo.elf n64-sysinfo.z64

.PHONY: all clean
