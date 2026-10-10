#include "../dmcu.h"
#include <assert.h>
#include <stdio.h>

static void load_words(Dmcu *cpu, unsigned segment,
                       const uint16_t *words, size_t count) {
    uint8_t bytes[256];
    size_t i;
    assert(count * 2 <= sizeof(bytes));
    for (i = 0; i < count; ++i) {
        bytes[i * 2] = (uint8_t)words[i];
        bytes[i * 2 + 1] = (uint8_t)(words[i] >> 8);
    }
    assert(dmcu_load_code(cpu, segment << 16, bytes, count * 2) == 0);
}
int main(void) {
    Dmcu *cpu = dmcu_create();
    /* Set SP, load R0, call a far routine, add, return, compare and branch. */
    const uint16_t main_code[] = {
        0x68ff, 0x6000, 0x68f0, 0x0029,
        0xbd21, 0x0000,
        0x5610, /* CMP R1,#0: R1 starts as zero. */
        0xd101, /* Z branch over unsupported word. */
        0xffff, 0xc700
    };
    const uint16_t sub_code[] = {
        0x2e01, 0x5001, 0x2f00, 0xc620
    };
    unsigned i;
    assert(cpu);
    load_words(cpu, 0x20, main_code, sizeof(main_code) / 2);
    load_words(cpu, 0x21, sub_code, sizeof(sub_code) / 2);
    cpu->pc = 0x200000;
    for (i = 0; i < 10; ++i) assert(dmcu_step(cpu) == DMCU_OK);
    assert(cpu->gpr[0] == 0x2a);
    assert(cpu->gpr[15] == 0x6000);
    assert(cpu->gpr[11] == 0);
    assert(cpu->pc == 0x200014);
    assert(dmcu_step(cpu) == DMCU_NO_CODE);
    cpu->pc = 0x200010;
    assert(dmcu_step(cpu) == DMCU_UNSUPPORTED);
    assert(cpu->fault_word == 0xffff && cpu->fault_pc == 0x200010);
    dmcu_destroy(cpu);
    puts("DMCU first-slice test passed");
    return 0;
}
