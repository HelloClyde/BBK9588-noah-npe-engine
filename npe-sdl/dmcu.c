#include "dmcu.h"
#include <stdlib.h>
#include <string.h>

/* Compact first slice of the DMCU interpreter. See qemu-dmcu/target/dmcu/
 * helper.c for verified instruction semantics. Unsupported words stop. */
Dmcu *dmcu_create(void) {
    Dmcu *cpu = calloc(1, sizeof(*cpu));
    if (!cpu) return NULL;
    cpu->code = calloc(DMCU_CODE_BYTES, 1);
    if (!cpu->code) { free(cpu); return NULL; }
    /* One bitmap per byte prevents an uninitialized 0x0000 from being run. */
    cpu->mapped = calloc(DMCU_CODE_BYTES, 1);
    cpu->data = calloc(DMCU_CODE_BYTES, 1);
    if (!cpu->mapped || !cpu->data) {
        free(cpu->mapped); free(cpu->data); free(cpu->code); free(cpu);
        return NULL;
    }
    return cpu;
}
void dmcu_destroy(Dmcu *cpu) {
    if (!cpu) return;
    free(cpu->code);
    free(cpu->mapped);
    free(cpu->data);
    free(cpu);
}
int dmcu_load_code(Dmcu *cpu, uint32_t address,
                   const uint8_t *data, size_t size) {
    if (!cpu || !cpu->code || !data || address >= DMCU_CODE_BYTES ||
        size > DMCU_CODE_BYTES - address) return -1;
    memcpy(cpu->code + address, data, size);
    memset(cpu->mapped + address, 1, size);
    return 0;
}
uint16_t dmcu_ram_word(const Dmcu *cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] |
        (unsigned)cpu->data[(uint16_t)(address + 1)] << 8);
}
static void ram_store(Dmcu *cpu, uint16_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[(uint16_t)(address + 1)] = (uint8_t)(value >> 8);
}
static void ram_store_byte(Dmcu *cpu, uint16_t address, uint8_t value) {
    /* Internal X/Y RAM has the DMCU byte-lane swap. */
    cpu->data[address ^ 1u] = value;
}
static uint32_t data_physical(const Dmcu *cpu, uint16_t logical) {
    unsigned selector = logical & 0x8000u ? 0x11 : 0x10;
    return ((uint32_t)(cpu->sfr[selector] & 0x1ffu) << 15) |
           (logical & 0x7fffu);
}
static int data_store(Dmcu *cpu, uint16_t logical,
                      uint16_t value, unsigned bits) {
    uint32_t address = data_physical(cpu, logical);
    /* The board's high memory, ROM aliases and MMIO are not wired yet. */
    if (address >= DMCU_RAM_BYTES - (bits == 16)) return -1;
    if (bits == 8) cpu->data[address ^ 1u] = (uint8_t)value;
    else {
        cpu->data[address] = (uint8_t)value;
        cpu->data[address + 1] = (uint8_t)(value >> 8);
    }
    return 0;
}
static void push(Dmcu *cpu, uint16_t value) {
    cpu->gpr[15] -= 2;
    ram_store(cpu, cpu->gpr[15], value);
}
static uint16_t pop(Dmcu *cpu) {
    uint16_t value = dmcu_ram_word(cpu, cpu->gpr[15]);
    cpu->gpr[15] += 2;
    return value;
}
static uint16_t reg_read(const Dmcu *cpu, unsigned reg, unsigned bits) {
    if (bits == 16) return cpu->gpr[reg & 15];
    return reg & 1 ? cpu->gpr[reg >> 1] & 255 : cpu->gpr[reg >> 1] >> 8;
}
static void reg_write(Dmcu *cpu, unsigned reg, uint16_t value, unsigned bits) {
    if (bits == 16) cpu->gpr[reg & 15] = value;
    else if (reg & 1)
        cpu->gpr[reg >> 1] = (cpu->gpr[reg >> 1] & 0xff00) | (value & 255);
    else
        cpu->gpr[reg >> 1] = (cpu->gpr[reg >> 1] & 255) | (value << 8);
}
static uint16_t sfr_read(const Dmcu *cpu, unsigned reg, unsigned bits) {
    uint16_t value;
    if (reg >= 0xf0) return reg_read(cpu, reg - 0xf0, bits);
    if (reg == 1) value = (cpu->sfr[1] & ~15u) |
        ((cpu->flags & DMCU_Z) ? 1 : 0) |
        ((cpu->flags & DMCU_V) ? 2 : 0) |
        ((cpu->flags & DMCU_C) ? 4 : 0) |
        ((cpu->flags & DMCU_N) ? 8 : 0);
    else value = cpu->sfr[reg];
    return bits == 8 ? value & 255 : value;
}
static void sfr_write(Dmcu *cpu, unsigned reg, uint16_t value, unsigned bits) {
    if (reg >= 0xf0) { reg_write(cpu, reg - 0xf0, value, bits); return; }
    if (bits == 8) value = (cpu->sfr[reg] & 0xff00) | (value & 255);
    if (reg == 1) {
        cpu->sfr[1] = value;
        cpu->flags = ((value & 1) ? DMCU_Z : 0) |
                     ((value & 2) ? DMCU_V : 0) |
                     ((value & 4) ? DMCU_C : 0) |
                     ((value & 8) ? DMCU_N : 0);
    } else cpu->sfr[reg] = value;
}
static int condition(const Dmcu *cpu, unsigned code) {
    int z = !!(cpu->flags & DMCU_Z), c = !!(cpu->flags & DMCU_C);
    int n = !!(cpu->flags & DMCU_N), v = !!(cpu->flags & DMCU_V);
    switch (code) {
    case 0: return 1; case 1: return z; case 2: return !z;
    case 3: return v; case 4: return !v; case 5: return c;
    case 6: return !c; case 7: return n; case 8: return !n;
    case 9: return n != v; case 10: return !z && !c;
    case 11: return n == v && !z; case 12: return z || c;
    case 13: return n != v || z; case 14: return n == v;
    default: return 0;
    }
}
static uint16_t alu(Dmcu *cpu, unsigned op, uint16_t left,
                    uint16_t right, unsigned bits) {
    uint32_t mask = bits == 8 ? 255u : 65535u;
    uint32_t sign = bits == 8 ? 128u : 32768u;
    uint32_t result = 0, rhs = right;
    int old_z = !!(cpu->flags & DMCU_Z);
    left &= mask; right &= mask;
    if (op == 0 || op == 1) {
        rhs += op == 1 && !!(cpu->flags & DMCU_C);
        result = left + rhs;
        cpu->flags = (result > mask ? DMCU_C : 0) |
            ((~(left ^ right) & (left ^ result) & sign) ? DMCU_V : 0) |
            ((result & sign) ? DMCU_N : 0) |
            (((result & mask) == 0 && (op != 1 || old_z)) ? DMCU_Z : 0);
    } else if (op == 2 || op == 3 || op == 6) {
        rhs += op == 3 && !!(cpu->flags & DMCU_C);
        result = left - rhs;
        cpu->flags = (left < rhs ? DMCU_C : 0) |
            (((left ^ right) & (left ^ result) & sign) ? DMCU_V : 0) |
            ((result & sign) ? DMCU_N : 0) |
            (((result & mask) == 0 && (op != 3 || old_z)) ? DMCU_Z : 0);
    } else {
        if (op == 4) result = left & right;
        if (op == 5) result = left | right;
        if (op == 7) result = left ^ right;
        cpu->flags = ((result & mask) == 0 ? DMCU_Z : 0) |
                     ((result & sign) ? DMCU_N : 0);
    }
    return (uint16_t)(result & mask);
}
static int is_wide(uint8_t op, uint8_t arg) {
    if ((op & 15) <= 7 && op >= 0x60 && op <= 0xb7) return 1;
    if (op == 0x68 || op == 0x69 || op == 0xac || op == 0xbc ||
        op == 0xad || op == 0xbd || op == 0xae || op == 0xbf)
        return op != 0xbf || arg == 0;
    return 0;
}
DmcuResult dmcu_step(Dmcu *cpu) {
    uint32_t start, next, target;
    uint16_t word, ext = 0, result;
    uint8_t op, arg;
    unsigned hi, low, bits, dst;
    if (!cpu || !cpu->code || !cpu->mapped) return DMCU_NO_CODE;
    start = cpu->pc & 0xffffffu;
    if (!cpu->mapped[start] || !cpu->mapped[(start & 0xff0000u) |
                                  ((start + 1) & 0xffffu)]) return DMCU_NO_CODE;
    word = cpu->code[start] | (uint16_t)cpu->code[(start & 0xff0000u) |
                                                ((start + 1) & 0xffffu)] << 8;
    op = (uint8_t)(word >> 8); arg = (uint8_t)word;
    if (is_wide(op, arg)) {
        uint32_t a = (start & 0xff0000u) | ((start + 2) & 0xffffu);
        uint32_t b = (start & 0xff0000u) | ((start + 3) & 0xffffu);
        if (!cpu->mapped[a] || !cpu->mapped[b]) return DMCU_NO_CODE;
        ext = cpu->code[a] | (uint16_t)cpu->code[b] << 8;
    }
    next = (start & 0xff0000u) | ((start + (is_wide(op, arg) ? 4 : 2)) & 0xffffu);
    cpu->pc = next;
    hi = op >> 4; low = op & 15;
    if (low <= 7 && (hi <= 1 || hi == 4 || hi == 5 || hi == 6 || hi == 7)) {
        bits = hi == 0 || hi == 4 || hi == 6 ? 8 : 16;
        if (hi <= 1) {
            dst = arg >> 4;
            result = alu(cpu, low, reg_read(cpu, dst, bits),
                         reg_read(cpu, arg & 15, bits), bits);
            if (low != 6) reg_write(cpu, dst, result, bits);
        } else if (hi <= 5) {
            dst = arg >> 4;
            result = alu(cpu, low, reg_read(cpu, dst, bits), arg & 15, bits);
            if (low != 6) reg_write(cpu, dst, result, bits);
        } else {
            result = alu(cpu, low, sfr_read(cpu, arg, bits), ext, bits);
            if (low != 6) sfr_write(cpu, arg, result, bits);
        }
    } else if (op == 0x68) sfr_write(cpu, arg, ext, 16);
    else if (op == 0x69) sfr_write(cpu, arg, ext, 8);
    else if (op == 0x48 || op == 0x49)
        reg_write(cpu, arg >> 4, arg & 15, op == 0x49 ? 8 : 16);
    else if (op == 0x08 || op == 0x09)
        reg_write(cpu, arg >> 4, reg_read(cpu, arg & 15,
                  op == 0x09 ? 8 : 16), op == 0x09 ? 8 : 16);
    else if (op == 0xa8 || op == 0xa9) {
        bits = op == 0xa9 ? 8 : 16;
        if (data_store(cpu, ext, sfr_read(cpu, arg, bits), bits)) {
            cpu->pc = start;
            cpu->fault_pc = start;
            cpu->fault_word = word;
            return DMCU_NO_DEVICE;
        }
    }
    else if (op == 0x58 || op == 0x59) {
        dst = arg >> 4;
        bits = op == 0x59 ? 8 : 16;
        if (dst < 11) goto unsupported;
        cpu->gpr[dst] -= (uint16_t)(bits / 8);
        result = reg_read(cpu, arg & 15, bits);
        if (bits == 8) ram_store_byte(cpu, cpu->gpr[dst], (uint8_t)result);
        else ram_store(cpu, cpu->gpr[dst], result);
    }
    else if ((op == 0xac || op == 0xbc) && (arg >> 4) == 0) {
        if (condition(cpu, arg & 15)) {
            if (op == 0xbc) push(cpu, (uint16_t)next);
            cpu->pc = (start & 0xff0000u) | ext;
        }
    } else if (op == 0xad || op == 0xbd) {
        if (op == 0xbd) { push(cpu, (uint16_t)next); push(cpu, (uint16_t)(next >> 16)); }
        cpu->pc = ((uint32_t)arg << 16) | ext;
    } else if (op >= 0xd0 && op <= 0xde) {
        if (condition(cpu, op & 15))
            cpu->pc = (start & 0xff0000u) |
                ((next + (int8_t)arg * 2) & 0xffffu);
    } else if (op == 0xc6 && arg == 0) {
        cpu->pc = (start & 0xff0000u) | pop(cpu);
    } else if (op == 0xc6 && arg == 0x20) {
        target = pop(cpu); cpu->pc = (target << 16) | pop(cpu);
    } else if (op == 0x2e) {
        push(cpu, cpu->gpr[11]); cpu->gpr[11] = cpu->gpr[15];
        cpu->gpr[15] -= arg * 2;
    } else if (op == 0x2f && arg == 0) {
        cpu->gpr[15] = cpu->gpr[11]; cpu->gpr[11] = pop(cpu);
    } else if (op == 0xc7 && arg == 0) {
        /* NOP */
    } else {
unsupported:
        cpu->pc = start; cpu->fault_pc = start; cpu->fault_word = word;
        return DMCU_UNSUPPORTED;
    }
    cpu->steps++;
    return DMCU_OK;
}
