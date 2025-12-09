#include "cpu_revision.h"

const char* get_cpu_revision(uint32_t prid) {
    uint8_t rev = prid & 0xFF;
    switch(rev) {
        case 0x00: return "VR4300 Rev 1.0";
        case 0x01: return "VR4300 Rev 2.0";
        case 0x02: return "VR4300 Rev 3.0";
        default: return "VR4300 Unknown";
    }
}
