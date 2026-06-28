// s64_cpu_top.v
// S64 CPU top level
// 5-stage pipeline: IF -> ID -> EX -> MEM -> WB
// Forwarding, load-use stall, branch flush
// Two buses: IBUS (instructions) DBUS (data)
// Rev1: S32 mode, 32-bit datapath, no hard FPU

`include "s64_opcodes.vh"

module s64_cpu_top (
    input  wire        clk,
    input  wire        rst,

    // IBUS — instruction bus
    output wire [31:0] ibus_addr,
    input  wire [31:0] ibus_rdata,
    output wire        ibus_re,
    input  wire        ibus_ready,

    // DBUS — data bus
    output wire [31:0] dbus_addr,
    output wire [31:0] dbus_wdata,
    output wire        dbus_we,
    output wire        dbus_re,
    output wire [3:0]  dbus_bsel,
    input  wire [31:0] dbus_rdata,
    input  wire        dbus_ready,

    // status
    output wire        halted,
    output wire [3:0]  fault
);

    // ── IF/ID pipeline register wires ────────────────────────────
    wire [31:0] if_id_pc;
    wire [31:0] if_id_instr;
    wire        if_id_valid;

    // ── ID/EX pipeline register wires ────────────────────────────
    wire [31:0] id_ex_pc;
    wire [7:0]  id_ex_opcode;
    wire [4:0]  id_ex_rd;
    wire [4:0]  id_ex_rs1;
    wire [4:0]  id_ex_rs2;
    wire [31:0] id_ex_rs1_val;
    wire [31:0] id_ex_rs2_val;
    wire [7:0]  id_ex_imm8;
    wire        id_ex_m;
    wire        id_ex_valid;

    // ── EX/MEM pipeline register wires ───────────────────────────
    wire [31:0] ex_mem_pc;
    wire [7:0]  ex_mem_opcode;
    wire [4:0]  ex_mem_rd;
    wire [31:0] ex_mem_alu_result;
    wire [31:0] ex_mem_rs1_val;
    wire [7:0]  ex_mem_imm8;
    wire        ex_mem_m;
    wire        ex_mem_is_load;
    wire        ex_mem_valid;

    // ── MEM/WB pipeline register wires ───────────────────────────
    wire [31:0] mem_wb_pc;
    wire [7:0]  mem_wb_opcode;
    wire [4:0]  mem_wb_rd;
    wire [31:0] mem_wb_result;
    wire        mem_wb_valid;

    // ── control signals ───────────────────────────────────────────
    wire        id_stall;
    wire        mem_stall;
    wire        stall = id_stall || mem_stall;
    wire        branch_taken;
    wire [31:0] branch_pc;

    // ── forwarding wires ──────────────────────────────────────────
    wire        fwd_wb_en;
    wire [4:0]  fwd_wb_rd;
    wire [31:0] fwd_wb_val;

    // EX->EX forwarding: result from EX/MEM reg
    wire        fwd_ex_en  = ex_mem_valid && (ex_mem_rd != 5'd31) && !ex_mem_is_load;
    wire [4:0]  fwd_ex_rd  = ex_mem_rd;
    wire [31:0] fwd_ex_val = ex_mem_alu_result;

    // MEM->EX forwarding: result from MEM/WB reg
    wire        fwd_mem_en  = fwd_wb_en;
    wire [4:0]  fwd_mem_rd  = fwd_wb_rd;
    wire [31:0] fwd_mem_val = fwd_wb_val;

    // ── register file ─────────────────────────────────────────────
    wire [4:0]  rf_raddr_a, rf_raddr_b;
    wire [31:0] rf_rdata_a, rf_rdata_b;
    wire        rf_wen;
    wire [4:0]  rf_waddr;
    wire [31:0] rf_wdata;

    s64_regfile regfile (
        .clk      (clk),
        .rst      (rst),
        .raddr_a  (rf_raddr_a),
        .rdata_a  (rf_rdata_a),
        .raddr_b  (rf_raddr_b),
        .rdata_b  (rf_rdata_b),
        .wen      (rf_wen),
        .waddr    (rf_waddr),
        .wdata    (rf_wdata)
    );

    // ── stage 1: IF ───────────────────────────────────────────────
    s64_if stage_if (
        .clk          (clk),
        .rst          (rst),
        .stall        (stall),
        .flush        (branch_taken),
        .branch_pc    (branch_pc),
        .branch_taken (branch_taken),
        .ibus_addr    (ibus_addr),
        .ibus_rdata   (ibus_rdata),
        .ibus_re      (ibus_re),
        .ibus_ready   (ibus_ready),
        .if_id_pc     (if_id_pc),
        .if_id_instr  (if_id_instr),
        .if_id_valid  (if_id_valid)
    );

    // ── stage 2: ID ───────────────────────────────────────────────
    s64_id stage_id (
        .clk          (clk),
        .rst          (rst),
        .if_id_pc     (if_id_pc),
        .if_id_instr  (if_id_instr),
        .if_id_valid  (if_id_valid),
        .rf_raddr_a   (rf_raddr_a),
        .rf_raddr_b   (rf_raddr_b),
        .rf_rdata_a   (rf_rdata_a),
        .rf_rdata_b   (rf_rdata_b),
        .ex_is_load   (ex_mem_is_load),
        .ex_rd        (ex_mem_rd),
        .stall        (id_stall),
        .flush        (branch_taken),
        .id_ex_pc     (id_ex_pc),
        .id_ex_opcode (id_ex_opcode),
        .id_ex_rd     (id_ex_rd),
        .id_ex_rs1    (id_ex_rs1),
        .id_ex_rs2    (id_ex_rs2),
        .id_ex_rs1_val(id_ex_rs1_val),
        .id_ex_rs2_val(id_ex_rs2_val),
        .id_ex_imm8   (id_ex_imm8),
        .id_ex_m      (id_ex_m),
        .id_ex_valid  (id_ex_valid)
    );

    // ── stage 3: EX ───────────────────────────────────────────────
    s64_ex stage_ex (
        .clk              (clk),
        .rst              (rst),
        .id_ex_pc         (id_ex_pc),
        .id_ex_opcode     (id_ex_opcode),
        .id_ex_rd         (id_ex_rd),
        .id_ex_rs1        (id_ex_rs1),
        .id_ex_rs2        (id_ex_rs2),
        .id_ex_rs1_val    (id_ex_rs1_val),
        .id_ex_rs2_val    (id_ex_rs2_val),
        .id_ex_imm8       (id_ex_imm8),
        .id_ex_m          (id_ex_m),
        .id_ex_valid      (id_ex_valid),
        .fwd_ex_val       (fwd_ex_val),
        .fwd_ex_rd        (fwd_ex_rd),
        .fwd_ex_en        (fwd_ex_en),
        .fwd_mem_val      (fwd_mem_val),
        .fwd_mem_rd       (fwd_mem_rd),
        .fwd_mem_en       (fwd_mem_en),
        .branch_taken     (branch_taken),
        .branch_pc        (branch_pc),
        .fault            (fault),
        .ex_mem_pc        (ex_mem_pc),
        .ex_mem_opcode    (ex_mem_opcode),
        .ex_mem_rd        (ex_mem_rd),
        .ex_mem_alu_result(ex_mem_alu_result),
        .ex_mem_rs1_val   (ex_mem_rs1_val),
        .ex_mem_imm8      (ex_mem_imm8),
        .ex_mem_m         (ex_mem_m),
        .ex_mem_is_load   (ex_mem_is_load),
        .ex_mem_valid     (ex_mem_valid)
    );

    // ── stage 4: MEM ──────────────────────────────────────────────
    s64_mem stage_mem (
        .clk              (clk),
        .rst              (rst),
        .ex_mem_pc        (ex_mem_pc),
        .ex_mem_opcode    (ex_mem_opcode),
        .ex_mem_rd        (ex_mem_rd),
        .ex_mem_alu_result(ex_mem_alu_result),
        .ex_mem_rs1_val   (ex_mem_rs1_val),
        .ex_mem_is_load   (ex_mem_is_load),
        .ex_mem_valid     (ex_mem_valid),
        .dbus_addr        (dbus_addr),
        .dbus_wdata       (dbus_wdata),
        .dbus_we          (dbus_we),
        .dbus_re          (dbus_re),
        .dbus_bsel        (dbus_bsel),
        .dbus_rdata       (dbus_rdata),
        .dbus_ready       (dbus_ready),
        .mem_stall        (mem_stall),
        .mem_wb_pc        (mem_wb_pc),
        .mem_wb_opcode    (mem_wb_opcode),
        .mem_wb_rd        (mem_wb_rd),
        .mem_wb_result    (mem_wb_result),
        .mem_wb_valid     (mem_wb_valid)
    );

    // ── stage 5: WB ───────────────────────────────────────────────
    s64_wb stage_wb (
        .clk          (clk),
        .rst          (rst),
        .mem_wb_pc    (mem_wb_pc),
        .mem_wb_opcode(mem_wb_opcode),
        .mem_wb_rd    (mem_wb_rd),
        .mem_wb_result(mem_wb_result),
        .mem_wb_valid (mem_wb_valid),
        .rf_wen       (rf_wen),
        .rf_waddr     (rf_waddr),
        .rf_wdata     (rf_wdata),
        .fwd_wb_en    (fwd_wb_en),
        .fwd_wb_rd    (fwd_wb_rd),
        .fwd_wb_val   (fwd_wb_val),
        .halted       (halted)
    );

endmodule
