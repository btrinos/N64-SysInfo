#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu_revision.h"

int main(void) {
    printf("Testing get_cpu_revision()...\n");

    assert(strcmp(get_cpu_revision(0x00), "VR4300 Rev 1.0") == 0);
    assert(strcmp(get_cpu_revision(0x01), "VR4300 Rev 2.0") == 0);
    assert(strcmp(get_cpu_revision(0x02), "VR4300 Rev 3.0") == 0);
    assert(strcmp(get_cpu_revision(0xFF), "VR4300 Unknown") == 0);

    // Test that higher bits are properly masked
    assert(strcmp(get_cpu_revision(0x0B00), "VR4300 Rev 1.0") == 0);
    assert(strcmp(get_cpu_revision(0x0B01), "VR4300 Rev 2.0") == 0);
    assert(strcmp(get_cpu_revision(0x0B02), "VR4300 Rev 3.0") == 0);

    printf("All tests passed!\n");
    return 0;
}
