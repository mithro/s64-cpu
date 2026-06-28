// s64_id.v
// S64 pipeline stage 2: Instruction Decode
// Decodes instruction fields, reads register file
// Detects load-use hazards, generates stall signal

`include "s64_opcodes.vh"

module s64_id (
    input  wire        clk,
    input  wire        rst,

    // from IF/ID
    input  wire [31:0] if_id_pc,
    input  wire [31:0] if_id_instr,
    input  wire        if_id_valid,

    // register file ports
    output wire [4:0]  rf_raddr_a,
    output wire [4:0]  rf_raddr_b,
    input  wire [31:0] rf_rdata_a,
    input  wire [31:0] rf_rdata_b,

    // from EX/MEM (for load-use hazard detection)
    input  wire        ex_is_load,
    input  wire [4:0]  ex_rd,

    // stall output — tells IF to hold
    output wire        stall,

    // flush input — invalidate this stage
    input  wire        flush,

    // ID/EX pipeline register outputs
    output reg  [31:0] id_ex_pc,
    output reg  [7:0]  id_ex_opcode,
    output reg  [4:0]  id_ex_rd,
    output reg  [4:0]  id_ex_rs1,
    output reg  [4:0]  id_ex_rs2,
    output reg  [31:0] id_ex_rs1_val,
    output reg  [31:0] id_ex_rs2_val,
    output reg  [7:0]  id_ex_imm8,
    output reg         id_ex_m,        // mode bit
    output reg         id_ex_valid
);

    // decode fields — purely combinational
    wire [7:0]  opcode = if_id_instr[31:24];
    wire [4:0]  rd     = if_id_instr[23:19];
    wire [4:0]  rs1    = if_id_instr[18:14];
    wire [4:0]  rs2    = if_id_instr[13:9];
    wire [7:0]  imm8   = if_id_instr[8:1];
    wire        m      = if_id_instr[0];

    assign rf_raddr_a = rs1;
    assign rf_raddr_b = rs2;

    // load-use hazard: EX stage is a load and its Rd matches our RS1 or RS2
    wire load_use_hazard = ex_is_load &&
                           ((ex_rd == rs1) || (!m && ex_rd == rs2)) &&
                           ex_rd != 5'd31;

    assign stall = load_use_hazard && if_id_valid;

    always @(posedge clk) begin
        if (rst || flush) begin
            id_ex_pc      <= 32'h0;
            id_ex_opcode  <= `OP_NOP;
            id_ex_rd      <= 5'h0;
            id_ex_rs1     <= 5'h0;
            id_ex_rs2     <= 5'h0;
            id_ex_rs1_val <= 32'h0;
            id_ex_rs2_val <= 32'h0;
            id_ex_imm8    <= 8'h0;
            id_ex_m       <= 1'b0;
            id_ex_valid   <= 1'b0;
        end else if (stall) begin
            // insert bubble
            id_ex_opcode  <= `OP_NOP;
            id_ex_valid   <= 1'b0;
        end else begin
            id_ex_pc      <= if_id_pc;
            id_ex_opcode  <= opcode;
            id_ex_rd      <= rd;
            id_ex_rs1     <= rs1;
            id_ex_rs2     <= rs2;
            id_ex_rs1_val <= rf_rdata_a;
            id_ex_rs2_val <= rf_rdata_b;
            id_ex_imm8    <= imm8;
            id_ex_m       <= m;
            id_ex_valid   <= if_id_valid;
        end
    end

endmodule
