module mod1 (
    input logic a,
    output logic b
);
assign b = a;
endmodule

module top;
logic l1, l2;

mod1 inst1 (.a(l1), .b(l2));
mod1 inst2 (.a(l2), .b(l1));
endmodule
