#ifndef NPE_DMCU_H
#define NPE_DMCU_H
#include <stddef.h>
#include <stdint.h>

enum { DMCU_CODE_BYTES = 0x1000000, DMCU_RAM_BYTES = 0x10000 };
enum { DMCU_Z = 1, DMCU_C = 2, DMCU_N = 4, DMCU_V = 8 };
typedef enum { DMCU_OK = 0, DMCU_UNSUPPORTED = -1,
               DMCU_NO_CODE = -2, DMCU_NO_DEVICE = -3 } DmcuResult;
typedef struct {
    uint8_t *code;
    uint8_t *mapped;
    uint8_t *data;
    uint16_t gpr[16];
    uint16_t sfr[256];
    uint16_t flags;
    uint32_t pc;
    uint16_t fault_word;
    uint32_t fault_pc;
    uint64_t steps;
} Dmcu;

Dmcu *dmcu_create(void);
void dmcu_destroy(Dmcu *cpu);
/* Unmapped code is rejected; a loaded 0x0000 word remains executable data. */
int dmcu_load_code(Dmcu *cpu, uint32_t address, const uint8_t *data, size_t size);
DmcuResult dmcu_step(Dmcu *cpu);
uint16_t dmcu_ram_word(const Dmcu *cpu, uint16_t address);
#endif
