module mod1 (
    input logic a,
    output logic b
);
logic c;
endmodule

module mod2 (
    input logic a,
    output logic b
);

mod1 inst (.*);
endmodule

module top;

logic a, b;

mod2 inst (.*);

endmodule
