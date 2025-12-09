#include <libdragon.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "cpu_revision.h"

// Memory map addresses for N64 hardware info
#define MI_VERSION_REG  0xA4300004
#define MI_INTR_REG     0xA4300008
#define VI_CURRENT_REG  0xA4400004
#define VI_STATUS_REG   0xA4400000
#define AI_DRAM_ADDR    0xA4500000
#define AI_STATUS       0xA450000C
#define PI_STATUS_REG   0xA4600010
#define PI_BSD_DOM1_LAT 0xA4600014
#define SI_STATUS_REG   0xA4800018
#define RI_MODE_REG     0xA4700000
#define RI_CONFIG_REG   0xA4700004
#define RI_SELECT_REG   0xA470000C

// COP0 registers
#define C0_COUNT 9
#define C0_PRID  15

// Tab system
typedef enum {
    TAB_CPU = 0,
    TAB_MEMORY,
    TAB_RCP,
    TAB_VIDEO,
    TAB_COUNT
} Tab;

const char* tab_names[TAB_COUNT] = {
    "CPU",
    "Memory", 
    "RCP",
    "Video"
};

// Measurement structure for continuous monitoring
typedef struct {
    // CPU measurements
    float cpu_freq_current;
    float cpu_freq_min;
    float cpu_freq_max;
    uint32_t cpu_cycles_per_frame;
    
    // Memory measurements
    uint32_t rdram_bandwidth;  // MB/s
    uint32_t rdram_latency;    // cycles
    
    // RCP measurements
    float rsp_load_percent;
    float rdp_load_percent;
    uint32_t vi_interrupts_per_sec;
    
    // Video measurements
    uint32_t current_scanline;
    float actual_fps;
    
    // Timing
    uint32_t frames_counted;
    uint32_t last_count;
} SystemMeasurements;

static SystemMeasurements measurements = {0};

// Read COP0 register
static inline uint32_t read_c0_count(void) {
    uint32_t count;
    asm volatile("mfc0 %0, $9" : "=r"(count));
    return count;
}

static inline uint32_t read_c0_prid(void) {
    uint32_t prid;
    asm volatile("mfc0 %0, $15" : "=r"(prid));
    return prid;
}

// Detect memory size
uint32_t detect_memory_size(void) {
    // Use libdragon's safe memory detection
    // Returns size in bytes, convert to MB
    return get_memory_size() / (1024 * 1024);
}

// Get TV type as human readable string
const char* get_tv_type_string(void) {
    switch(get_tv_type()) {
        case TV_PAL: return "PAL";
        case TV_NTSC: return "NTSC";
        case TV_MPAL: return "MPAL";
        default: return "Unknown";
    }
}

float get_tv_refresh_rate(void) {
    switch(get_tv_type()) {
        case TV_PAL: return 50.0f;
        case TV_NTSC: return 60.0f;
        case TV_MPAL: return 60.0f;
        default: return 60.0f;
    }
}

// Read RCP version
uint32_t get_rcp_version(void) {
    volatile uint32_t *mi_version = (uint32_t *)MI_VERSION_REG;
    return *mi_version;
}

// Read RDRAM configuration
uint32_t get_rdram_config(void) {
    volatile uint32_t *ri_config = (uint32_t *)RI_CONFIG_REG;
    return *ri_config;
}

// Measure CPU frequency continuously
void measure_cpu_frequency_continuous(void) {
    static uint32_t last_frame_count = 0;
    static uint32_t measure_start_count = 0;
    static int measuring = 0;
    
    uint32_t current_count = read_c0_count();
    
    if (!measuring) {
        measure_start_count = current_count;
        last_frame_count = measurements.frames_counted;
        measuring = 1;
        return;
    }
    
    uint32_t frames_elapsed = measurements.frames_counted - last_frame_count;
    
    // Update every 5 frames for accuracy
    if (frames_elapsed >= 5) {
        uint32_t count_delta = current_count - measure_start_count;
        uint32_t cpu_cycles = count_delta * 2; // COUNT is half CPU speed
        
        float frame_rate = get_tv_refresh_rate();
        float cpu_freq = ((float)cpu_cycles / (float)frames_elapsed) * frame_rate / 1000000.0f;
        
        measurements.cpu_freq_current = cpu_freq;
        measurements.cpu_cycles_per_frame = cpu_cycles / frames_elapsed;
        
        // Track min/max
        if (measurements.cpu_freq_min == 0 || cpu_freq < measurements.cpu_freq_min) {
            measurements.cpu_freq_min = cpu_freq;
        }
        if (cpu_freq > measurements.cpu_freq_max) {
            measurements.cpu_freq_max = cpu_freq;
        }
        
        measuring = 0;
    }
}

// Measure memory bandwidth (approximate via timing)
void measure_memory_bandwidth(void) {
    static uint32_t last_measure_frame = 0;
    
    if (measurements.frames_counted - last_measure_frame < 30) {
        return; // Measure every 30 frames (0.5 sec)
    }
    
    last_measure_frame = measurements.frames_counted;
    
    // Allocate dedicated uncached buffers to avoid stomping code/data
    // Use uncached memory (KSEG1: 0xA0000000) to avoid cache effects
    #define BW_TEST_SIZE 4096  // 4KB test
    static uint32_t src_buffer[BW_TEST_SIZE / 4] __attribute__((aligned(16)));
    static uint32_t dst_buffer[BW_TEST_SIZE / 4] __attribute__((aligned(16)));
    
    // Use uncached addresses to bypass cache
    volatile uint32_t *src = (volatile uint32_t *)((uintptr_t)src_buffer | 0x20000000);
    volatile uint32_t *dst = (volatile uint32_t *)((uintptr_t)dst_buffer | 0x20000000);
    
    // Initialize source buffer
    for (int i = 0; i < BW_TEST_SIZE / 4; i++) {
        src[i] = i;
    }
    
    // Flush data cache to ensure clean test
    data_cache_hit_writeback_invalidate(src_buffer, BW_TEST_SIZE);
    data_cache_hit_writeback_invalidate(dst_buffer, BW_TEST_SIZE);
    
    uint32_t count_start = read_c0_count();
    
    // Copy 4KB using uncached access (tests actual RDRAM speed)
    for (int i = 0; i < BW_TEST_SIZE / 4; i++) {
        dst[i] = src[i];
    }
    
    uint32_t count_end = read_c0_count();
    uint32_t cycles = (count_end - count_start) * 2;
    
    // Calculate bandwidth: bytes / (cycles / CPU_freq)
    if (measurements.cpu_freq_current > 0) {
        float time_seconds = (float)cycles / (measurements.cpu_freq_current * 1000000.0f);
        float bandwidth_mbps = (BW_TEST_SIZE / time_seconds) / (1024.0f * 1024.0f);
        measurements.rdram_bandwidth = (uint32_t)bandwidth_mbps;
    }
    #undef BW_TEST_SIZE
}

// Measure current video scanline
void measure_video_scanline(void) {
    volatile uint32_t *vi_current = (uint32_t *)VI_CURRENT_REG;
    measurements.current_scanline = (*vi_current >> 1) & 0x3FF;
}

// Calculate actual FPS
void calculate_fps(void) {
    static uint32_t last_fps_frame = 0;
    static uint32_t last_fps_count = 0;
    static int first_sample = 1;
    
    // Initialize on first call
    if (first_sample && measurements.frames_counted >= 60) {
        last_fps_frame = measurements.frames_counted;
        last_fps_count = read_c0_count();
        first_sample = 0;
        return;
    }

    uint32_t frames_elapsed = measurements.frames_counted - last_fps_frame;

    if (frames_elapsed >= 60) {
        uint32_t current_count = read_c0_count();
        uint32_t count_delta = current_count - last_fps_count;
        uint32_t cpu_cycles = count_delta * 2;

        if (measurements.cpu_freq_current > 0 && frames_elapsed > 0) {
            float time_seconds = (float)cpu_cycles / (measurements.cpu_freq_current * 1000000.0f);
            measurements.actual_fps = frames_elapsed / time_seconds;
        }

        last_fps_frame = measurements.frames_counted;
        last_fps_count = current_count;
    }
}

// Update all measurements (called every frame)
void update_measurements(void) {
    measurements.frames_counted++;

    measure_cpu_frequency_continuous();
    measure_video_scanline();

    if (measurements.frames_counted % 60 == 0) {
        calculate_fps();
    }
    
    // Less frequent measurements
    if (measurements.frames_counted % 30 == 0) {
        measure_memory_bandwidth();
    }
}

// Draw a labeled value
void draw_label_value(display_context_t disp, int x, int y, const char* label, const char* value) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%-20s : %s", label, value);
    graphics_draw_text(disp, x, y, buffer);
}

// Draw CPU tab
void draw_cpu_tab(display_context_t disp, uint32_t prid, uint32_t memory_mb) {
    int y = 50;
    int line_height = 11;
    char buffer[128];
    
    // Processor section
    graphics_draw_text(disp, 15, y, "Processor");
    y += line_height + 2;
    
    draw_label_value(disp, 20, y, "Name", "MIPS VR4300i");
    y += line_height;
    
    snprintf(buffer, sizeof(buffer), "%s", get_cpu_revision(prid));
    draw_label_value(disp, 20, y, "Revision", buffer);
    y += line_height;
    
    snprintf(buffer, sizeof(buffer), "0x%08X", prid);
    draw_label_value(disp, 20, y, "Code Name", buffer);
    y += line_height;
    
    draw_label_value(disp, 20, y, "Package", "Single-Chip");
    y += line_height;
    
    draw_label_value(disp, 20, y, "Technology", "0.35um / 0.18um");
    y += line_height;
    
    y += 3;
    
    // Specification section
    graphics_draw_text(disp, 15, y, "Specification");
    y += line_height + 2;
    
    draw_label_value(disp, 20, y, "Instruction Set", "MIPS III (64-bit)");
    y += line_height;
    
    y += 3;
    
    // Clocks section - REAL-TIME MEASUREMENTS
    graphics_draw_text(disp, 15, y, "Clocks (Real-Time)");
    y += line_height + 2;
    
    snprintf(buffer, sizeof(buffer), "%.2f MHz", measurements.cpu_freq_current);
    draw_label_value(disp, 20, y, "Core Speed", buffer);
    y += line_height;
    
    draw_label_value(disp, 20, y, "Multiplier", "x1.0");
    y += line_height;
    
    snprintf(buffer, sizeof(buffer), "%.2f MHz", measurements.cpu_freq_current);
    draw_label_value(disp, 20, y, "Bus Speed", buffer);
    y += line_height;
    
    y += 3;
    
    // Cache section
    graphics_draw_text(disp, 15, y, "Cache");
    y += line_height + 2;
    
    draw_label_value(disp, 20, y, "L1 Data", "16 KB");
    y += line_height;

    draw_label_value(disp, 20, y, "L1 Instruction", "16 KB");
    y += line_height;
    
    y += 3;
    
    // Frequency range
    graphics_draw_text(disp, 15, y, "Frequency Range");
    y += line_height + 2;
    
    snprintf(buffer, sizeof(buffer), "%.2f MHz", measurements.cpu_freq_min);
    draw_label_value(disp, 20, y, "Min", buffer);
    y += line_height;
    
    snprintf(buffer, sizeof(buffer), "%.2f MHz", measurements.cpu_freq_max);
    draw_label_value(disp, 20, y, "Max", buffer);
    y += line_height;
}

// Draw Memory tab
void draw_memory_tab(display_context_t disp, uint32_t memory_mb) {
    int y = 50;
    int line_height = 11;
    char buffer[128];
    
    // General section
    graphics_draw_text(disp, 15, y, "General");
    y += line_height + 2;
    
    draw_label_value(disp, 20, y, "Type", "Rambus DRAM");
    y += line_height;
    
    snprintf(buffer, sizeof(buffer), "%u MB", memory_mb);
    draw_label_value(disp, 20, y, "Size", buffer);
    y += line_height;
    
    const char* expansion = (memory_mb == 8) ? "Yes" : "No";
    draw_label_value(disp, 20, y, "Expansion Pak", expansion);
    y += line_height;
    
    y += 3;
    
    // Timings section - REAL-TIME
    graphics_draw_text(disp, 15, y, "Timings (Real-Time)");
    y += line_height + 2;
    
    draw_label_value(disp, 20, y, "Frequency", "250 MHz");
    y += line_height;
    
    snprintf(buffer, sizeof(buffer), "%u MB/s", measurements.rdram_bandwidth);
    draw_label_value(disp, 20, y, "Bandwidth", buffer);
    y += line_height;
    
    draw_label_value(disp, 20, y, "Bus Width", "9-bit");
    y += line_height;
    
    draw_label_value(disp, 20, y, "Theoretical Max", "562 MB/s");
    y += line_height;
    
    y += 3;
    
    // Physical Memory Map
    graphics_draw_text(disp, 15, y, "Physical Memory");
    y += line_height + 2;
    
    draw_label_value(disp, 20, y, "Base RDRAM", "0x00000000-0x003FFFFF");
    y += line_height;
    
    if (memory_mb == 8) {
        draw_label_value(disp, 20, y, "Expansion", "0x00400000-0x007FFFFF");
        y += line_height;
    }
    
    draw_label_value(disp, 20, y, "MMIO Start", "0x04000000");
    y += line_height;
}

// Draw RCP tab
void draw_rcp_tab(display_context_t disp, uint32_t rcp_version) {
    int y = 50;
    int line_height = 11;
    char buffer[128];
    
    // RCP General
    graphics_draw_text(disp, 15, y, "Reality Co-Processor");
    y += line_height + 2;
    
    snprintf(buffer, sizeof(buffer), "0x%08X", rcp_version);
    draw_label_value(disp, 20, y, "Version", buffer);
    y += line_height;
    
    draw_label_value(disp, 20, y, "Clock", "62.5 MHz");
    y += line_height;
    
    y += 3;
    
    // RSP Section
    graphics_draw_text(disp, 15, y, "RSP (Reality Signal Processor)");
    y += line_height + 2;
    
    draw_label_value(disp, 20, y, "Type", "Vector Processor");
    y += line_height;
    
    draw_label_value(disp, 20, y, "Clock", "62.5 MHz");
    y += line_height;
    
    draw_label_value(disp, 20, y, "DMEM", "4 KBytes");
    y += line_height;
    
    draw_label_value(disp, 20, y, "IMEM", "4 KBytes");
    y += line_height;
    
    draw_label_value(disp, 20, y, "Vector Unit", "8 x 128-bit regs");
    y += line_height;
    
    y += 3;
    
    // RDP Section
    graphics_draw_text(disp, 15, y, "RDP (Reality Display Processor)");
    y += line_height + 2;
    
    draw_label_value(disp, 20, y, "Type", "Rasterizer");
    y += line_height;
    
    draw_label_value(disp, 20, y, "Clock", "62.5 MHz");
    y += line_height;
    
    draw_label_value(disp, 20, y, "TMEM", "4 KBytes");
    y += line_height;
    
    draw_label_value(disp, 20, y, "Fill Rate", "~100 Mpixels/s");
    y += line_height;
    
    draw_label_value(disp, 20, y, "Texture Formats", "Multiple");
    y += line_height;
}

// Draw Video tab
void draw_video_tab(display_context_t disp) {
    int y = 50;
    int line_height = 11;
    char buffer[128];
    
    // Video Interface
    graphics_draw_text(disp, 15, y, "Video Interface");
    y += line_height + 2;
    
    snprintf(buffer, sizeof(buffer), "%s", get_tv_type_string());
    draw_label_value(disp, 20, y, "TV System", buffer);
    y += line_height;
    
    snprintf(buffer, sizeof(buffer), "%.1f Hz", get_tv_refresh_rate());
    draw_label_value(disp, 20, y, "Refresh Rate", buffer);
    y += line_height;
    
    y += 3;
    
    // Current Settings
    graphics_draw_text(disp, 15, y, "Current Mode");
    y += line_height + 2;
    
    draw_label_value(disp, 20, y, "Resolution", "320 x 240");
    y += line_height;
    
    draw_label_value(disp, 20, y, "Color Depth", "32-bit RGBA");
    y += line_height;
    
    draw_label_value(disp, 20, y, "Pixel Format", "RGBA 8888");
    y += line_height;
    
    y += 3;
    
    // Real-time measurements
    graphics_draw_text(disp, 15, y, "Real-Time Status");
    y += line_height + 2;
    
    snprintf(buffer, sizeof(buffer), "%u", measurements.current_scanline);
    draw_label_value(disp, 20, y, "Current Scanline", buffer);
    y += line_height;
    
    snprintf(buffer, sizeof(buffer), "%.1f fps", measurements.actual_fps);
    draw_label_value(disp, 20, y, "Actual FPS", buffer);
    y += line_height;
    
    snprintf(buffer, sizeof(buffer), "%u", measurements.frames_counted);
    draw_label_value(disp, 20, y, "Frame Count", buffer);
    y += line_height;
}

int main(void) {
    // Initialize display
    display_init(RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    
    // Initialize controller
    controller_init();
    
    // Get static system information
    uint32_t prid = read_c0_prid();
    uint32_t memory_mb = detect_memory_size();
    uint32_t rcp_version = get_rcp_version();
    
    // Initialize measurements
    measurements.cpu_freq_current = 93.75f;
    measurements.cpu_freq_min = 93.75f;
    measurements.cpu_freq_max = 93.75f;
    measurements.frames_counted = 0;
    measurements.rdram_bandwidth = 500; // Initial estimate
    measurements.actual_fps = get_tv_refresh_rate(); // Initialize to expected refresh rate
    
    Tab current_tab = TAB_CPU;
    
    while(1) {
        // Update all real-time measurements
        update_measurements();
        
        // Scan for controller input
        controller_scan();
        struct controller_data keys = get_keys_down();
        
        // Tab navigation
        if(keys.c[0].C_left || keys.c[0].L) {
            current_tab = (current_tab - 1 + TAB_COUNT) % TAB_COUNT;
        }
        if(keys.c[0].C_right || keys.c[0].R) {
            current_tab = (current_tab + 1) % TAB_COUNT;
        }
        
        // Exit on Start
        if(keys.c[0].start) {
            break;
        }
        
        // Lock display
        display_context_t disp = 0;
        while(!(disp = display_lock()));
        
        // Clear screen with dark background
        graphics_fill_screen(disp, 0x1A1A2EFF);
        
        // Draw title bar
        graphics_draw_box(disp, 0, 0, 320, 25, 0x2D2D44FF);
        graphics_draw_text(disp, 10, 8, "N64-Z - Nintendo 64 System Info");
        
        // Draw tabs
        for (int i = 0; i < TAB_COUNT; i++) {
            int tab_x = 10 + (i * 70);
            int tab_y = 28;
            
            if (i == current_tab) {
                // Active tab
                graphics_draw_box(disp, tab_x, tab_y, 65, 18, 0x4A4A6AFF);
                graphics_draw_text(disp, tab_x + 5, tab_y + 5, tab_names[i]);
            } else {
                // Inactive tab
                graphics_draw_box(disp, tab_x, tab_y, 65, 18, 0x2D2D44FF);
                graphics_draw_text(disp, tab_x + 5, tab_y + 5, tab_names[i]);
            }
        }
        
        // Draw current tab content
        switch(current_tab) {
            case TAB_CPU:
                draw_cpu_tab(disp, prid, memory_mb);
                break;
            case TAB_MEMORY:
                draw_memory_tab(disp, memory_mb);
                break;
            case TAB_RCP:
                draw_rcp_tab(disp, rcp_version);
                break;
            case TAB_VIDEO:
                draw_video_tab(disp);
                break;
        }
        
        // Draw status bar
        graphics_draw_box(disp, 0, 225, 320, 15, 0x2D2D44FF);
        graphics_draw_text(disp, 10, 229, "L/R: Switch Tab | START: Exit");
        
        // Show display
        display_show(disp);
    }
    
    return 0;
}
