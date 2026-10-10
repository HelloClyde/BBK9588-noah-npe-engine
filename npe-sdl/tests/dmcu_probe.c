#include "../dmcu.h"
#include <stdio.h>
#include <stdlib.h>

/* Developer probe: execute the factory firmware from VMA03 until the first
 * unsupported instruction or unmapped code. No peripheral is simulated. */
int main(int argc, char **argv) {
    FILE *file;
    long length;
    uint8_t *bytes;
    Dmcu *cpu;
    DmcuResult result = DMCU_OK;
    unsigned i;
    if (argc != 2) { fprintf(stderr, "usage: dmcu-probe firmware.prg\n"); return 2; }
    file = fopen(argv[1], "rb");
    if (!file || fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
        (unsigned long)length > 0x400000u || fseek(file, 0, SEEK_SET)) {
        fprintf(stderr, "cannot read firmware\n");
        if (file) fclose(file);
        return 2;
    }
    bytes = malloc((size_t)length);
    cpu = dmcu_create();
    if (!bytes || !cpu || fread(bytes, 1, (size_t)length, file) != (size_t)length ||
        dmcu_load_code(cpu, 0x30000, bytes, (size_t)length)) {
        fprintf(stderr, "cannot load firmware\n");
        free(bytes); dmcu_destroy(cpu); fclose(file); return 2;
    }
    fclose(file); free(bytes);
    cpu->pc = 0x30000;
    cpu->gpr[15] = 0xfffe;
    cpu->sfr[0x11] = 1;
    for (i = 0; i < 1000000; ++i) {
        result = dmcu_step(cpu);
        if (result != DMCU_OK) break;
    }
    printf("steps=%llu pc=%02X:%04X result=%d word=%04X R0=%04X SP=%04X\n",
           (unsigned long long)cpu->steps, cpu->pc >> 16, cpu->pc & 0xffff,
           result, cpu->fault_word, cpu->gpr[0], cpu->gpr[15]);
    dmcu_destroy(cpu);
    return result == DMCU_OK ? 0 : 1;
}
