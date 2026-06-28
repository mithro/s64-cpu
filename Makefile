CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2 -I include
LDFLAGS = -lm

LIB_SRC = lib/cpu.c lib/memory.c lib/loader.c lib/disasm.c
LIB_OBJ = $(LIB_SRC:.c=.o)
LIB     = libS64.a

EMU_SRC = emu/main.c
EMU     = s64emu

AS_SRC  = as64/main.c as64/lexer.c as64/parser.c as64/encoder.c as64/symbols.c
AS      = as64

LD_SRC  = ld64/main.c ld64/obj.c ld64/resolve.c ld64/reloc.c ld64/emit.c
LD_BIN  = ld64

.PHONY: all lib emu as64 ld64 clean

all: lib emu as64

lib: $(LIB)

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

emu: lib $(EMU_SRC)
	$(CC) $(CFLAGS) $(EMU_SRC) -L. -lS64 $(LDFLAGS) -o $(EMU)

as64: $(AS_SRC)
	$(CC) $(CFLAGS) $(AS_SRC) -o $(AS)

ld64: $(LD_SRC)
	$(CC) $(CFLAGS) $(LD_SRC) -o $(LD_BIN)

clean:
	rm -f $(LIB_OBJ) $(LIB) $(EMU) $(AS) $(LD_BIN)
