// s64_regfile.v
// S64 register file
// 32 x 32-bit registers (S32 mode for rev1 tapeout)
// 2 async read ports, 1 sync write port
// R31 hardwired to zero — reads always 0, writes discarded
// SISC Rule 2: no implicit operands — enforced here in hardware

module s64_regfile (
    input  wire        clk,
    input  wire        rst,

    // read port A (RS1)
    input  wire [4:0]  raddr_a,
    output wire [31:0] rdata_a,

    // read port B (RS2)
    input  wire [4:0]  raddr_b,
    output wire [31:0] rdata_b,

    // write port
    input  wire        wen,
    input  wire [4:0]  waddr,
    input  wire [31:0] wdata
);

    // register storage — R0 to R30
    // R31 is NOT stored, always returns 0
    reg [31:0] regs [0:30];

    integer i;

    // synchronous write — R31 writes are silently discarded
    always @(posedge clk) begin
        if (rst) begin
            for (i = 0; i < 31; i = i + 1)
                regs[i] <= 32'h0;
        end else if (wen && waddr != 5'd31) begin
            regs[waddr] <= wdata;
        end
    end

    // asynchronous reads — R31 hardwired to 0
    assign rdata_a = (raddr_a == 5'd31) ? 32'h0 : regs[raddr_a];
    assign rdata_b = (raddr_b == 5'd31) ? 32'h0 : regs[raddr_b];

endmodule
