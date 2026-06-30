// s64_if.v
// S64 pipeline stage 1: Instruction Fetch
// Fetches 32-bit instruction from IBUS at current PC
// Stalls when ibus_ready is low
// Flushes on taken branch (flush signal from EX stage)

module s64_if (
    input  wire        clk,
    input  wire        rst,

    // control
    input  wire        stall,       // stall from MEM (load-use hazard)
    input  wire        flush,       // flush from EX (branch taken)
    input  wire [31:0] branch_pc,   // new PC on branch/jump
    input  wire        branch_taken,

    // IBUS (instruction bus)
    output reg  [31:0] ibus_addr,
    input  wire [31:0] ibus_rdata,
    output wire        ibus_re,
    input  wire        ibus_ready,

    // IF/ID pipeline register outputs
    output reg  [31:0] if_id_pc,
    output reg  [31:0] if_id_instr,
    output reg         if_id_valid
);

    reg [31:0] pc;

    assign ibus_re = 1'b1; // always fetching

    always @(posedge clk) begin
        if (rst) begin
            pc           <= `MEM_RAM_BASE;
            ibus_addr    <= `MEM_RAM_BASE;
            if_id_pc     <= 32'h0;
            if_id_instr  <= 32'h0;
            if_id_valid  <= 1'b0;
        end else if (flush) begin
            // branch taken — redirect PC, invalidate in-flight instr
            pc           <= branch_pc;
            ibus_addr    <= branch_pc;
            if_id_valid  <= 1'b0;
            if_id_instr  <= 32'h0;
        end else if (stall || !ibus_ready) begin
            // stall — hold everything, do not advance PC
            if_id_valid  <= 1'b0;
        end else begin
            // normal fetch
            if_id_pc     <= pc;
            if_id_instr  <= ibus_rdata;
            if_id_valid  <= 1'b1;
            pc           <= pc + 32'd4;
            ibus_addr    <= pc + 32'd4;
        end
    end

endmodule
