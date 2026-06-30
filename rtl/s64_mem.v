// s64_mem.v
// S64 pipeline stage 4: Memory Access
// Handles LD/ST via DBUS
// Stalls pipeline while waiting for DBUS ready

`include "s64_opcodes.vh"

module s64_mem (
    input  wire        clk,
    input  wire        rst,

    // from EX/MEM
    input  wire [31:0] ex_mem_pc,
    input  wire [7:0]  ex_mem_opcode,
    input  wire [4:0]  ex_mem_rd,
    input  wire [31:0] ex_mem_alu_result,  // mem addr for LD/ST, ALU result otherwise
    input  wire [31:0] ex_mem_rs1_val,     // store data
    input  wire        ex_mem_is_load,
    input  wire        ex_mem_valid,

    // DBUS (data bus)
    output reg  [31:0] dbus_addr,
    output reg  [31:0] dbus_wdata,
    output reg         dbus_we,
    output reg         dbus_re,
    output reg  [3:0]  dbus_bsel,      // byte select
    input  wire [31:0] dbus_rdata,
    input  wire        dbus_ready,

    // stall output (back to IF/ID/EX while waiting for memory)
    output wire        mem_stall,

    // MEM/WB pipeline register outputs
    output reg  [31:0] mem_wb_pc,
    output reg  [7:0]  mem_wb_opcode,
    output reg  [4:0]  mem_wb_rd,
    output reg  [31:0] mem_wb_result,   // ALU result or loaded data
    output reg         mem_wb_valid
);

    assign mem_stall = (dbus_re || dbus_we) && !dbus_ready;

    wire is_store = (ex_mem_opcode == `OP_ST64 || ex_mem_opcode == `OP_ST32 ||
                     ex_mem_opcode == `OP_ST16 || ex_mem_opcode == `OP_ST8);

    always @(*) begin
        dbus_addr  = ex_mem_alu_result;
        dbus_wdata = ex_mem_rs1_val;
        dbus_we    = 1'b0;
        dbus_re    = 1'b0;
        dbus_bsel  = 4'hF;

        if (ex_mem_valid) begin
            case (ex_mem_opcode)
                `OP_LD32: begin dbus_re = 1'b1; dbus_bsel = 4'b0011; end
                `OP_LD16: begin dbus_re = 1'b1; dbus_bsel = 4'b0001; end
                `OP_LD8:  begin dbus_re = 1'b1; dbus_bsel = 4'b0001; end
                `OP_LD64: begin dbus_re = 1'b1; dbus_bsel = 4'hF;    end
                `OP_ST32: begin dbus_we = 1'b1; dbus_bsel = 4'b0011; end
                `OP_ST16: begin dbus_we = 1'b1; dbus_bsel = 4'b0001; end
                `OP_ST8:  begin dbus_we = 1'b1; dbus_bsel = 4'b0001; end
                `OP_ST64: begin dbus_we = 1'b1; dbus_bsel = 4'hF;    end
                default:  ;
            endcase
        end
    end

    // loaded data with zero-extension
    wire [31:0] load_result =
        (ex_mem_opcode == `OP_LD8)  ? {24'h0, dbus_rdata[7:0]}   :
        (ex_mem_opcode == `OP_LD16) ? {16'h0, dbus_rdata[15:0]}  :
        dbus_rdata;

    always @(posedge clk) begin
        if (rst) begin
            mem_wb_pc     <= 32'h0;
            mem_wb_opcode <= `OP_NOP;
            mem_wb_rd     <= 5'h0;
            mem_wb_result <= 32'h0;
            mem_wb_valid  <= 1'b0;
        end else if (!mem_stall) begin
            mem_wb_pc     <= ex_mem_pc;
            mem_wb_opcode <= ex_mem_opcode;
            mem_wb_rd     <= ex_mem_rd;
            mem_wb_result <= ex_mem_is_load ? load_result : ex_mem_alu_result;
            mem_wb_valid  <= ex_mem_valid;
        end
    end

endmodule
