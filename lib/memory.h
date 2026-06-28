#ifndef S64_MEMORY_H
#define S64_MEMORY_H

#include "../include/s64.h"

int  mem_init(void);
void mem_free(void);

/* raw buffer access */
int  mem_write_buf(u64 addr, const u8 *buf, u64 size);
int  mem_read_buf(u64 addr, u8 *buf, u64 size);

/* typed little-endian reads */
u8   mem_read8(u64 addr);
u16  mem_read16(u64 addr);
u32  mem_read32(u64 addr);
u64  mem_read64(u64 addr);

/* typed little-endian writes */
void mem_write8(u64 addr, u8 val);
void mem_write16(u64 addr, u16 val);
void mem_write32(u64 addr, u32 val);
void mem_write64(u64 addr, u64 val);

/* debug */
void mem_dump(u64 addr, u64 size);

#endif /* S64_MEMORY_H */
