module mod1 (
    input logic a, b,
    output logic c
);
logic d;
endmodule

module top;
logic a, b, c;
mod1 inst (.*);
endmodule
