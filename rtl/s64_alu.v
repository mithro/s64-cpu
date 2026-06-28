// s64_alu.v
// S64 ALU — integer arithmetic, bitwise, compare
// Pure combinational — no state
// One instruction, one effect (SISC Rule 1)

`include "s64_opcodes.vh"

module s64_alu (
    input  wire [7:0]  opcode,
    input  wire [31:0] a,        // RS1 value
    input  wire [31:0] b,        // RS2 value or sign-extended imm8
    output reg  [31:0] result,
    output reg         valid     // 1 if this opcode is handled by ALU
);

    always @(*) begin
        valid  = 1'b1;
        result = 32'h0;

        case (opcode)
            // integer ALU
            `OP_ADD,  `OP_ADDI: result = a + b;
            `OP_SUB,  `OP_SUBI: result = a - b;
            `OP_MUL,  `OP_MULI: result = a * b;
            `OP_DIV:             result = ($signed(a) / $signed(b));
            `OP_MOD:             result = ($signed(a) % $signed(b));

            // bitwise
            `OP_AND:             result = a & b;
            `OP_OR:              result = a | b;
            `OP_XOR:             result = a ^ b;
            `OP_NOT:             result = ~a;
            `OP_SHL, `OP_SHLI:  result = a << b[4:0];
            `OP_SHR, `OP_SHRI:  result = a >> b[4:0];
            `OP_SAR:             result = $signed(a) >>> b[4:0];

            // compare — result is 1 or 0 written to Rd
            `OP_EQ:  result = (a == b)               ? 32'h1 : 32'h0;
            `OP_NE:  result = (a != b)               ? 32'h1 : 32'h0;
            `OP_LT:  result = ($signed(a) <  $signed(b)) ? 32'h1 : 32'h0;
            `OP_GT:  result = ($signed(a) >  $signed(b)) ? 32'h1 : 32'h0;
            `OP_LTE: result = ($signed(a) <= $signed(b)) ? 32'h1 : 32'h0;
            `OP_GTE: result = ($signed(a) >= $signed(b)) ? 32'h1 : 32'h0;
            `OP_LTU: result = (a <  b)               ? 32'h1 : 32'h0;
            `OP_GTU: result = (a >  b)               ? 32'h1 : 32'h0;

            // MOV
            `OP_MOV:             result = a;
            `OP_MOVI, `OP_MOVW: result = b;

            default: begin
                result = 32'h0;
                valid  = 1'b0;
            end
        endcase
    end

endmodule
