#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../src/cpu_revision.h"

int main(void) {
    assert(strcmp(get_cpu_revision((uint32_t)0x00), "VR4300 Rev 1.0") == 0);
    assert(strcmp(get_cpu_revision((uint32_t)0x01), "VR4300 Rev 2.0") == 0);
    assert(strcmp(get_cpu_revision((uint32_t)0x02), "VR4300 Rev 3.0") == 0);
    assert(strcmp(get_cpu_revision((uint32_t)0xFF), "VR4300 Unknown") == 0);

    return 0;
}
