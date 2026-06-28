#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* flat memory — one big array for the emulator */
#define MEM_SIZE (256 * 1024 * 1024)   /* 256MB emulated RAM */

static u8 *mem_data = NULL;

int mem_init(void) {
    mem_data = calloc(1, MEM_SIZE);
    if (!mem_data) {
        fprintf(stderr, "s64emu: failed to allocate memory\n");
        return -1;
    }
    return 0;
}

void mem_free(void) {
    free(mem_data);
    mem_data = NULL;
}

/* bounds check */
static int mem_check(u64 addr, u64 size) {
    if (addr + size > MEM_SIZE) {
        fprintf(stderr, "s64emu: bad address 0x%016llx\n",
                (unsigned long long)addr);
        return -1;
    }
    return 0;
}

/* ─── raw byte access ─────────────────────────────────────────── */
int mem_write_buf(u64 addr, const u8 *buf, u64 size) {
    if (mem_check(addr, size)) return -1;
    memcpy(mem_data + addr, buf, size);
    return 0;
}

int mem_read_buf(u64 addr, u8 *buf, u64 size) {
    if (mem_check(addr, size)) return -1;
    memcpy(buf, mem_data + addr, size);
    return 0;
}

/* ─── little-endian typed reads ───────────────────────────────── */
u8 mem_read8(u64 addr) {
    if (mem_check(addr, 1)) return 0;
    return mem_data[addr];
}

u16 mem_read16(u64 addr) {
    if (mem_check(addr, 2)) return 0;
    return (u16)mem_data[addr]
         | (u16)mem_data[addr+1] << 8;
}

u32 mem_read32(u64 addr) {
    if (mem_check(addr, 4)) return 0;
    return (u32)mem_data[addr]
         | (u32)mem_data[addr+1] << 8
         | (u32)mem_data[addr+2] << 16
         | (u32)mem_data[addr+3] << 24;
}

u64 mem_read64(u64 addr) {
    if (mem_check(addr, 8)) return 0;
    return (u64)mem_data[addr]
         | (u64)mem_data[addr+1] << 8
         | (u64)mem_data[addr+2] << 16
         | (u64)mem_data[addr+3] << 24
         | (u64)mem_data[addr+4] << 32
         | (u64)mem_data[addr+5] << 40
         | (u64)mem_data[addr+6] << 48
         | (u64)mem_data[addr+7] << 56;
}

/* ─── little-endian typed writes ──────────────────────────────── */
void mem_write8(u64 addr, u8 val) {
    if (mem_check(addr, 1)) return;
    mem_data[addr] = val;
}

void mem_write16(u64 addr, u16 val) {
    if (mem_check(addr, 2)) return;
    mem_data[addr]   = val & 0xFF;
    mem_data[addr+1] = (val >> 8) & 0xFF;
}

void mem_write32(u64 addr, u32 val) {
    if (mem_check(addr, 4)) return;
    mem_data[addr]   = val & 0xFF;
    mem_data[addr+1] = (val >> 8)  & 0xFF;
    mem_data[addr+2] = (val >> 16) & 0xFF;
    mem_data[addr+3] = (val >> 24) & 0xFF;
}

void mem_write64(u64 addr, u64 val) {
    if (mem_check(addr, 8)) return;
    mem_data[addr]   = val & 0xFF;
    mem_data[addr+1] = (val >> 8)  & 0xFF;
    mem_data[addr+2] = (val >> 16) & 0xFF;
    mem_data[addr+3] = (val >> 24) & 0xFF;
    mem_data[addr+4] = (val >> 32) & 0xFF;
    mem_data[addr+5] = (val >> 40) & 0xFF;
    mem_data[addr+6] = (val >> 48) & 0xFF;
    mem_data[addr+7] = (val >> 56) & 0xFF;
}

/* ─── hex dump (debug) ────────────────────────────────────────── */
void mem_dump(u64 addr, u64 size) {
    u64 i;
    for (i = 0; i < size; i++) {
        if (i % 16 == 0) printf("\n%016llx: ", (unsigned long long)(addr + i));
        printf("%02x ", mem_read8(addr + i));
    }
    printf("\n");
}
