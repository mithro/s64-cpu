// s64_wb.v
// S64 pipeline stage 5: Writeback
// Writes result back to register file
// Generates forwarding signals for EX stage

`include "s64_opcodes.vh"

module s64_wb (
    input  wire        clk,
    input  wire        rst,

    // from MEM/WB
    input  wire [31:0] mem_wb_pc,
    input  wire [7:0]  mem_wb_opcode,
    input  wire [4:0]  mem_wb_rd,
    input  wire [31:0] mem_wb_result,
    input  wire        mem_wb_valid,

    // register file write port
    output reg         rf_wen,
    output reg  [4:0]  rf_waddr,
    output reg  [31:0] rf_wdata,

    // forwarding outputs
    output wire        fwd_wb_en,
    output wire [4:0]  fwd_wb_rd,
    output wire [31:0] fwd_wb_val,

    // halt signal
    output reg         halted
);

    // opcodes that write to Rd
    wire does_writeback =
        (mem_wb_opcode != `OP_ST64) && (mem_wb_opcode != `OP_ST32) &&
        (mem_wb_opcode != `OP_ST16) && (mem_wb_opcode != `OP_ST8)  &&
        (mem_wb_opcode != `OP_NOP)  && (mem_wb_opcode != `OP_HLT)  &&
        (mem_wb_opcode != `OP_SYS)  && (mem_wb_opcode != `OP_JMP)  &&
        (mem_wb_opcode != `OP_JMPI) && (mem_wb_opcode != `OP_RET)  &&
        mem_wb_valid;

    assign fwd_wb_en  = does_writeback && (mem_wb_rd != 5'd31);
    assign fwd_wb_rd  = mem_wb_rd;
    assign fwd_wb_val = mem_wb_result;

    always @(posedge clk) begin
        if (rst) begin
            rf_wen  <= 1'b0;
            rf_waddr <= 5'h0;
            rf_wdata <= 32'h0;
            halted  <= 1'b0;
        end else begin
            rf_wen   <= does_writeback && (mem_wb_rd != 5'd31);
            rf_waddr <= mem_wb_rd;
            rf_wdata <= mem_wb_result;

            if (mem_wb_valid && mem_wb_opcode == `OP_HLT)
                halted <= 1'b1;
        end
    end

endmodule
