// s64_tb.v
// S64 CPU testbench
// Simulates the 5-stage pipeline with a simple program:
//   MOVI R0, #10
//   MOVI R1, #32
//   ADD  R2, R0, R1   (expect R2 = 42)
//   HLT

`include "s64_opcodes.vh"
`timescale 1ns/1ps

module s64_tb;

    // clock and reset
    reg clk = 0;
    reg rst = 1;
    always #10 clk = ~clk;  // 50MHz

    // IBUS
    wire [31:0] ibus_addr;
    wire        ibus_re;
    reg  [31:0] ibus_rdata;
    reg         ibus_ready;

    // DBUS
    wire [31:0] dbus_addr;
    wire [31:0] dbus_wdata;
    wire        dbus_we;
    wire        dbus_re;
    wire [3:0]  dbus_bsel;
    reg  [31:0] dbus_rdata;
    reg         dbus_ready;

    // status
    wire        halted;
    wire [3:0]  fault;

    // DUT
    s64_cpu_top dut (
        .clk        (clk),
        .rst        (rst),
        .ibus_addr  (ibus_addr),
        .ibus_rdata (ibus_rdata),
        .ibus_re    (ibus_re),
        .ibus_ready (ibus_ready),
        .dbus_addr  (dbus_addr),
        .dbus_wdata (dbus_wdata),
        .dbus_we    (dbus_we),
        .dbus_re    (dbus_re),
        .dbus_bsel  (dbus_bsel),
        .dbus_rdata (dbus_rdata),
        .dbus_ready (dbus_ready),
        .halted     (halted),
        .fault      (fault)
    );

    // simple instruction memory
    // base = 0x8000, instructions at 0x8000, 0x8004, 0x8008, 0x800C
    reg [31:0] imem [0:15];

    // encode macro
    `define ENC(op,rd,rs1,rs2,imm8,m) \
        ({op[7:0], rd[4:0], rs1[4:0], rs2[4:0], imm8[7:0], m[0]})

    initial begin
        // MOVI R0, #10   — opcode=0x49, rd=0, imm8=10, M=1
        imem[0] = {8'h49, 5'd0, 5'd0, 5'd0, 8'd10, 1'b1};
        // MOVI R1, #32   — opcode=0x49, rd=1, imm8=32, M=1
        imem[1] = {8'h49, 5'd1, 5'd0, 5'd0, 8'd32, 1'b1};
        // ADD  R2,R0,R1  — opcode=0x00, rd=2, rs1=0, rs2=1, M=0
        imem[2] = {8'h00, 5'd2, 5'd0, 5'd1, 8'd0,  1'b0};
        // HLT            — opcode=0x61
        imem[3] = {8'h61, 5'd0, 5'd0, 5'd0, 8'd0,  1'b0};
        // padding
        imem[4] = {8'h60, 5'd0, 5'd0, 5'd0, 8'd0, 1'b0}; // NOP
        imem[5] = {8'h60, 5'd0, 5'd0, 5'd0, 8'd0, 1'b0};
    end

    // instruction bus model — 1 cycle latency
    always @(*) begin
        ibus_ready = 1'b1;
        // address is PC, base 0x8000 -> index 0
        ibus_rdata = imem[(ibus_addr - 32'h8000) >> 2];
    end

    // data bus model — always ready, no data memory needed for this test
    always @(*) begin
        dbus_ready = 1'b1;
        dbus_rdata = 32'h0;
    end

    // test sequence
    integer cycle;
    initial begin
        $dumpfile("s64_tb.vcd");
        $dumpvars(0, s64_tb);

        // hold reset for 3 cycles
        rst = 1;
        repeat(3) @(posedge clk);
        rst = 0;

        $display("S64 pipeline simulation started");
        $display("Program: MOVI R0,#10 | MOVI R1,#32 | ADD R2,R0,R1 | HLT");
        $display("Expected: R2 = 42 (0x2A)");
        $display("");

        // run until halted or timeout
        cycle = 0;
        while (!halted && cycle < 100) begin
            @(posedge clk);
            cycle = cycle + 1;
        end

        @(posedge clk);

        if (fault != `FAULT_NONE) begin
            $display("FAULT: code=%0d", fault);
            $finish;
        end

        if (!halted) begin
            $display("TIMEOUT after %0d cycles", cycle);
            $finish;
        end

        $display("Halted after %0d cycles", cycle);

        // check register file directly
        $display("R0 = %0d (expect 10)",  dut.regfile.regs[0]);
        $display("R1 = %0d (expect 32)",  dut.regfile.regs[1]);
        $display("R2 = %0d (expect 42)",  dut.regfile.regs[2]);

        if (dut.regfile.regs[2] == 32'd42) begin
            $display("");
            $display("PASS: R2 = 42 correct!");
        end else begin
            $display("");
            $display("FAIL: R2 = %0d, expected 42", dut.regfile.regs[2]);
        end

        $finish;
    end

    // cycle logger
    always @(posedge clk) begin
        if (!rst)
            $display("cycle %3d | IF_PC=%08h | EX_OP=%02h | WB_RD=R%0d WB_VAL=%0d | halt=%b",
                     cycle,
                     dut.ibus_addr,
                     dut.stage_ex.id_ex_opcode,
                     dut.stage_wb.mem_wb_rd,
                     dut.stage_wb.mem_wb_result,
                     halted);
    end

endmodule
