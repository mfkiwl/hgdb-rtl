module mod1;
endmodule

module mod2;
mod1 inst2();
endmodule

module mod3;
mod2 inst3();
mod2 inst4();
endmodule

module top;
mod3 inst5();
mod3 inst6();
endmodule
