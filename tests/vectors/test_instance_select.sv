module mod1;
endmodule

module mod2;
mod1 inst();
endmodule

module mod3;
mod2 inst1();
mod2 inst2();
endmodule

module top;
mod3 inst1();
mod3 inst2();
endmodule
