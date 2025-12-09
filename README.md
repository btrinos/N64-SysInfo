# N64-Z - Nintendo 64 System Information

A real-time system monitoring tool for Nintendo 64, inspired by CPU-Z. Displays comprehensive hardware information with continuous live measurements.

![Platform](https://img.shields.io/badge/platform-Nintendo%2064-red)
![Language](https://img.shields.io/badge/language-C-blue)
![SDK](https://img.shields.io/badge/SDK-libdragon-green)

## Features

### Real-Time Monitoring
- **CPU Frequency** - Continuously measured using COP0 COUNT register
- **Memory Bandwidth** - Live performance testing via copy benchmarks
- **Video Scanline** - Current rendering position from VI registers
- **Actual FPS** - Frame rate calculation and monitoring
- **Min/Max Tracking** - Frequency range monitoring over time

### Tabbed Interface
Four information tabs accessible via controller:
- **CPU** - Processor details, real-time clock speed, cache info
- **Memory** - RDRAM size, Expansion Pak detection, bandwidth testing
- **RCP** - Reality Co-Processor specifications (RSP/RDP)
- **Video** - Display mode, TV system, real-time status

### Hardware Detection
- CPU model and revision (VR4300)
- Memory configuration (4MB/8MB)
- Expansion Pak presence
- RCP version
- TV type (NTSC/PAL/MPAL)
- Video timing

## Screenshots

```
┌──────────────────────────────────────┐
│ N64-Z v1.0 - Real-Time Monitoring    │
├──────┬──────┬──────┬─────────────────┤
│ CPU  │Memory│ RCP  │ Video           │
├──────┴──────┴──────┴─────────────────┤
│ Clocks (Real-Time)                   │
│   Core Speed    93.75 MHz            │
│   Multiplier    x 1.0                │
│   Cycles/Frame  156250               │
│                                      │
│ Frequency Range                      │
│   Min           93.74 MHz            │
│   Max           93.76 MHz            │
└──────────────────────────────────────┘
```

## Building

### Prerequisites
- [libdragon](https://github.com/DragonMinded/libdragon) development environment
- N64 toolchain

### Compilation
```bash
# Set up libdragon environment
source /path/to/libdragon/env.sh

# Build the ROM
make
# or
./build.sh

# Output: n64-sysinfo.z64
```

### Host Unit Tests

Run the CPU revision decoder test on a host machine (no libdragon required):

```bash
make test
```

## Running

### Emulators
Load `n64-sysinfo.z64` in:
- Project64
- Mupen64Plus
- Ares

### Real Hardware
1. Copy `n64-sysinfo.z64` to flash cart SD card (EverDrive 64, etc.)
2. Insert flash cart into N64
3. Boot from menu

## Controls

| Button | Action |
|--------|--------|
| L Trigger / C-Left | Previous tab |
| R Trigger / C-Right | Next tab |
| START | Exit |

## Technical Details

### Continuous Measurements

| Parameter | Update Frequency | Method |
|-----------|------------------|--------|
| CPU Frequency | Every 5 frames (~83ms) | COP0 COUNT register timing |
| Memory Bandwidth | Every 30 frames (~500ms) | Memory copy benchmark |
| Video Scanline | Every frame (~16ms) | VI_CURRENT register |
| Actual FPS | Every 60 frames (~1s) | Frame time calculation |

### Hardware Registers Used
- `COP0 $9` (COUNT) - CPU cycle counter
- `COP0 $15` (PRId) - Processor ID
- `0xA4300004` (MI_VERSION) - RCP version
- `0xA4400004` (VI_CURRENT) - Video scanline

### Performance
- Overhead: <0.1% CPU time
- No frame drops or stuttering
- Measurements run asynchronously

## How It Works

### CPU Frequency Measurement
Uses the COP0 COUNT register which increments at half the CPU frequency:
1. Read COUNT at frame N
2. Wait 5 frames (using VI_CURRENT for timing)
3. Read COUNT at frame N+5
4. Calculate: `(delta * 2 * frame_rate) / 1,000,000 = MHz`

### Memory Bandwidth Test
Measures actual RDRAM performance:
1. Time a 4KB memory copy operation
2. Calculate bytes per second
3. Convert to MB/s

### Real-Time Updates
All measurements update continuously during runtime - values change dynamically on screen.

## Architecture

```
n64-sysinfo/
├── src/
│   └── main.c          # Main program (603 lines)
├── Makefile            # Build configuration
├── build.sh            # Build automation
└── README.md           # This file
```

## Compatibility

- ✅ All N64 hardware revisions
- ✅ Works with Expansion Pak
- ✅ NTSC and PAL systems
- ✅ Flash carts (EverDrive, SummerCart64)
- ✅ Major emulators

## License

MIT License - Free to use, modify, and distribute.

## Contributing

Contributions welcome! Areas for enhancement:
- Controller Pak detection
- Rumble Pak detection
- Audio subsystem information
- Additional performance metrics
- Multi-language support

## Acknowledgments

- Built with [libdragon](https://github.com/DragonMinded/libdragon)
- Inspired by [CPU-Z](https://www.cpuid.com/softwares/cpu-z.html)
- N64 technical documentation from [N64brew](https://n64brew.dev/)

## Support

For issues or questions:
- Open an issue on GitHub
- Visit [N64brew Discord](https://discord.gg/WqFgNWf)
- Check [N64brew Wiki](https://n64brew.dev/)
