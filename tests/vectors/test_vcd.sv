module mod (
    input logic clk,
    input logic[15:0] a,
    output logic[15:0] b
);

always_ff @(posedge clk) begin
    b <= a;
end

endmodule


module top;

logic clk;
logic [15:0] a, b;

initial clk = 0;
always clk = #5 ~clk;

initial begin
    $dumpfile("test_vcd.vcd");
    $dumpvars(0, top);
    for (int i = 0; i < 10; i++) begin
        a = i;
        @(posedge clk);
    end
    $finish;
end

endmodule
