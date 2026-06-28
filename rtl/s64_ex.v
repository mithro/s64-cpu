// s64_ex.v
// S64 pipeline stage 3: Execute
// Runs ALU, resolves branches, handles forwarding
// Branch resolved here — flushes IF+ID on taken

`include "s64_opcodes.vh"

module s64_ex (
    input  wire        clk,
    input  wire        rst,

    // from ID/EX
    input  wire [31:0] id_ex_pc,
    input  wire [7:0]  id_ex_opcode,
    input  wire [4:0]  id_ex_rd,
    input  wire [4:0]  id_ex_rs1,
    input  wire [4:0]  id_ex_rs2,
    input  wire [31:0] id_ex_rs1_val,
    input  wire [31:0] id_ex_rs2_val,
    input  wire [7:0]  id_ex_imm8,
    input  wire        id_ex_m,
    input  wire        id_ex_valid,

    // forwarding inputs
    input  wire [31:0] fwd_ex_val,   // from EX/MEM
    input  wire [4:0]  fwd_ex_rd,
    input  wire        fwd_ex_en,
    input  wire [31:0] fwd_mem_val,  // from MEM/WB
    input  wire [4:0]  fwd_mem_rd,
    input  wire        fwd_mem_en,

    // branch/jump outputs (to IF)
    output reg         branch_taken,
    output reg  [31:0] branch_pc,

    // fault output
    output reg  [3:0]  fault,

    // EX/MEM pipeline register outputs
    output reg  [31:0] ex_mem_pc,
    output reg  [7:0]  ex_mem_opcode,
    output reg  [4:0]  ex_mem_rd,
    output reg  [31:0] ex_mem_alu_result,
    output reg  [31:0] ex_mem_rs1_val,  // for stores
    output reg  [7:0]  ex_mem_imm8,
    output reg         ex_mem_m,
    output reg         ex_mem_is_load,
    output reg         ex_mem_valid
);

    // sign-extend imm8
    wire [31:0] simm8 = {{24{id_ex_imm8[7]}}, id_ex_imm8};
    wire [31:0] uimm8 = {24'h0, id_ex_imm8};

    // forwarding mux for RS1
    wire [31:0] rs1_val =
        (fwd_ex_en  && fwd_ex_rd  == id_ex_rs1 && id_ex_rs1 != 5'd31) ? fwd_ex_val  :
        (fwd_mem_en && fwd_mem_rd == id_ex_rs1 && id_ex_rs1 != 5'd31) ? fwd_mem_val :
        id_ex_rs1_val;

    // forwarding mux for RS2
    wire [31:0] rs2_raw =
        (fwd_ex_en  && fwd_ex_rd  == id_ex_rs2 && id_ex_rs2 != 5'd31) ? fwd_ex_val  :
        (fwd_mem_en && fwd_mem_rd == id_ex_rs2 && id_ex_rs2 != 5'd31) ? fwd_mem_val :
        id_ex_rs2_val;

    // M-bit mux: use imm8 or RS2
    wire [31:0] operand_b = id_ex_m ? simm8 : rs2_raw;

    // ALU instantiation
    wire [31:0] alu_result;
    wire        alu_valid;

    s64_alu alu (
        .opcode (id_ex_opcode),
        .a      (rs1_val),
        .b      (operand_b),
        .result (alu_result),
        .valid  (alu_valid)
    );

    // is this a load instruction?
    wire is_load = (id_ex_opcode == `OP_LD64 || id_ex_opcode == `OP_LD32 ||
                    id_ex_opcode == `OP_LD16 || id_ex_opcode == `OP_LD8);

    // is this a store?
    wire is_store = (id_ex_opcode == `OP_ST64 || id_ex_opcode == `OP_ST32 ||
                     id_ex_opcode == `OP_ST16 || id_ex_opcode == `OP_ST8);

    // memory address = RS1 + uimm8
    wire [31:0] mem_addr = rs1_val + uimm8;

    // branch/jump logic (combinational)
    always @(*) begin
        branch_taken = 1'b0;
        branch_pc    = 32'h0;
        fault        = `FAULT_NONE;

        if (id_ex_valid) begin
            case (id_ex_opcode)
                `OP_JMP:
                    begin branch_taken = 1'b1; branch_pc = rs1_val; end
                `OP_JMPI:
                    begin
                        branch_taken = 1'b1;
                        branch_pc    = id_ex_pc + (simm8 << 2);
                    end
                `OP_JEQ:
                    if (rs2_raw == 32'h1)
                        begin branch_taken = 1'b1; branch_pc = rs1_val; end
                `OP_JNE:
                    if (rs2_raw == 32'h0)
                        begin branch_taken = 1'b1; branch_pc = rs1_val; end
                `OP_CALL:
                    begin branch_taken = 1'b1; branch_pc = rs1_val; end
                `OP_RET:
                    begin branch_taken = 1'b1; branch_pc = rs1_val; end
                `OP_LOOP:
                    if (rs1_val - 1 != 32'h0)
                        begin branch_taken = 1'b1; branch_pc = rs2_raw; end
                `OP_DIV, `OP_MOD:
                    if (operand_b == 32'h0) fault = `FAULT_DIV_ZERO;
                default: ;
            endcase
        end
    end

    always @(posedge clk) begin
        if (rst) begin
            ex_mem_pc         <= 32'h0;
            ex_mem_opcode     <= `OP_NOP;
            ex_mem_rd         <= 5'h0;
            ex_mem_alu_result <= 32'h0;
            ex_mem_rs1_val    <= 32'h0;
            ex_mem_imm8       <= 8'h0;
            ex_mem_m          <= 1'b0;
            ex_mem_is_load    <= 1'b0;
            ex_mem_valid      <= 1'b0;
        end else begin
            ex_mem_pc         <= id_ex_pc;
            ex_mem_opcode     <= id_ex_opcode;
            ex_mem_rd         <= id_ex_rd;
            ex_mem_alu_result <= (is_load || is_store) ? mem_addr : alu_result;
            ex_mem_rs1_val    <= rs1_val;   // store data
            ex_mem_imm8       <= id_ex_imm8;
            ex_mem_m          <= id_ex_m;
            ex_mem_is_load    <= is_load;
            ex_mem_valid      <= id_ex_valid;
        end
    end

endmodule
