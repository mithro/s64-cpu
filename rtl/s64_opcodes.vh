// s64_opcodes.vh
// S64 opcode defines for Verilog
// Matches s64.h exactly

// integer ALU
`define OP_ADD   8'h00
`define OP_SUB   8'h01
`define OP_MUL   8'h02
`define OP_DIV   8'h03
`define OP_MOD   8'h04
`define OP_ADDI  8'h05
`define OP_SUBI  8'h06
`define OP_MULI  8'h07

// bitwise & shift
`define OP_AND   8'h10
`define OP_OR    8'h11
`define OP_XOR   8'h12
`define OP_NOT   8'h13
`define OP_SHL   8'h14
`define OP_SHR   8'h15
`define OP_SAR   8'h16
`define OP_SHLI  8'h17
`define OP_SHRI  8'h18

// compare
`define OP_EQ    8'h20
`define OP_NE    8'h21
`define OP_LT    8'h22
`define OP_GT    8'h23
`define OP_LTE   8'h24
`define OP_GTE   8'h25
`define OP_LTU   8'h26
`define OP_GTU   8'h27

// control flow
`define OP_JMP   8'h30
`define OP_JMPI  8'h31
`define OP_JEQ   8'h32
`define OP_JNE   8'h33
`define OP_CALL  8'h34
`define OP_RET   8'h35
`define OP_LOOP  8'h36

// memory
`define OP_LD64  8'h40
`define OP_LD32  8'h41
`define OP_LD16  8'h42
`define OP_LD8   8'h43
`define OP_ST64  8'h44
`define OP_ST32  8'h45
`define OP_ST16  8'h46
`define OP_ST8   8'h47
`define OP_MOV   8'h48
`define OP_MOVI  8'h49
`define OP_MOVW  8'h4A

// floating point (emulated in rev1)
`define OP_FADD  8'h50
`define OP_FSUB  8'h51
`define OP_FMUL  8'h52
`define OP_FDIV  8'h53
`define OP_FCMP  8'h54
`define OP_FTOI  8'h55
`define OP_ITOF  8'h56

// system
`define OP_NOP   8'h60
`define OP_HLT   8'h61
`define OP_SYS   8'h62
`define OP_PUSH  8'h63
`define OP_POP   8'h64

// pipeline stage IDs
`define STAGE_IF  3'd1
`define STAGE_ID  3'd2
`define STAGE_EX  3'd3
`define STAGE_MEM 3'd4
`define STAGE_WB  3'd5

// fault codes
`define FAULT_NONE       4'd0
`define FAULT_ILLEGAL_OP 4'd1
`define FAULT_DIV_ZERO   4'd2
`define FAULT_BAD_ALIGN  4'd3
`define FAULT_BAD_ADDR   4'd4

// memory map
`define MEM_RESET_VEC    32'h00000000
`define MEM_NMI_VEC      32'h00000008
`define MEM_FAULT_VEC    32'h00000010
`define MEM_IRQ_VEC      32'h00000018
`define MEM_RAM_BASE     32'h00008000
`define MEM_FW_BASE      32'hFFFE0000
