#include <memory>

#include "../extern/slang/include/slang/syntax/SyntaxTree.h"
#include "../src/rtl.hh"
#include "gtest/gtest.h"
#include "fmt/format.h"

class TestDesignDatabase : public ::testing::Test {
protected:
    void load_str(const std::string_view &sv) {
        auto tree = slang::SyntaxTree::fromText(sv);
        compilation.addSyntaxTree(tree);
        design_ = std::make_unique<hgdb::rtl::DesignDatabase>(compilation);
    }

    slang::Compilation compilation;
    std::unique_ptr<hgdb::rtl::DesignDatabase> design_;
};

TEST_F(TestDesignDatabase, select) {  // NOLINT
    load_str(R"(
module child (
    input logic a, b,
    output logic c
);
logic d;
endmodule
module top;
    logic a, b, c;
    child inst (.*);
endmodule
)");

    auto const *p = design_->select("top.inst.a");
    EXPECT_NE(p, nullptr);

    auto const *inst = design_->select("top");
    EXPECT_NE(inst, nullptr);

    inst = design_->select("top.inst");
    EXPECT_NE(inst, nullptr);
    EXPECT_EQ(inst->name, "inst");

    auto const *inst_symbol = design_->get_instance("top.inst");
    EXPECT_NE(inst_symbol, nullptr);
}

TEST_F(TestDesignDatabase, port_connections) {  // NOLINT
    load_str(R"(
module child (
    input logic a, b, c,
    output logic d
);
endmodule
module top;
    logic a, b, d;
    child inst (.*, .c(a + b));
endmodule
)");

    auto const *inst = design_->get_instance("top.inst");
    auto connections = design_->get_connected_symbols(inst, "c");
    EXPECT_EQ(connections.size(), 2);
    connections = design_->get_connected_symbols("top.inst", "a");
    EXPECT_EQ(connections.size(), 1);
}

TEST_F(TestDesignDatabase, instance_definition) {  // NOLINT
    load_str(R"(
module child1;
endmodule
module child2;
child1 inst1();
endmodule
module top;
child1 inst1();
child2 inst2();
endmodule
)");

    auto const *inst = design_->get_instance("top.inst1");
    EXPECT_EQ(design_->get_instance_definition_name(inst), "child1");
    inst = design_->get_instance("top.inst2");
    EXPECT_EQ(design_->get_instance_definition_name(inst), "child2");
    inst = design_->get_instance("top.inst1");
    EXPECT_EQ(design_->get_instance_definition_name(inst), "child1");
}

TEST_F(TestDesignDatabase, get_instance_path) {  // NOLINT
    auto constexpr inst_path = "top.inst1";
    load_str(R"(
module child1;
endmodule
module top;
child1 inst1();
endmodule
)");

    auto const *inst = design_->get_instance(inst_path);
    EXPECT_NE(inst, nullptr);
    auto path = design_->get_instance_path(inst);
    EXPECT_EQ(path, inst_path);
}

TEST_F(TestDesignDatabase, is_inside) {  // NOLINT
    load_str(R"(
module child1;
endmodule
module child2;
child1 inst1();
endmodule
module top;
child1 inst1();
child2 inst2();
endmodule
)");

    auto const *inst1 = design_->get_instance("top.inst1");
    auto const *inst2 = design_->get_instance("top");
    EXPECT_TRUE(design_->instance_inside(inst1, inst2));
    auto const *inst3 = design_->get_instance("top.inst2.inst1");
    EXPECT_TRUE(design_->instance_inside(inst3, inst2));
    EXPECT_FALSE(design_->instance_inside(inst3, inst1));
}

TEST_F(TestDesignDatabase, get_source_instances_var) {  // NOLINT
    load_str(R"(
module mod1 (
  input logic a, b, c,
  output logic d
);
endmodule
module mod2 (
  input a,
  output b
);
endmodule
module top;
logic l1, l2, l3, l4, l5;
mod2 inst1(1'b1, l1);
mod2 inst2(1'b1, l2);
mod2 inst3(1'b1, l3);
mod1 inst4(l1, l2, l3, l4);
mod2 inst5(l4, l5);
endmodule
)");

    auto const *inst4 = design_->get_instance("top.inst4");
    auto sources = design_->get_source_instances(inst4);
    EXPECT_EQ(sources.size(), 3);

    for (auto i = 1; i < 4; i++) {
        auto name = fmt::format("top.inst{0}", i);
        auto const *inst = design_->get_instance(name);
        EXPECT_NE(inst, nullptr);
        EXPECT_NE(std::find(sources.begin(), sources.end(), inst), sources.end());
    }
    auto sinks = design_->get_sink_instances(inst4);
    auto const *inst5 = design_->get_instance("top.inst5");
    EXPECT_EQ(sinks.size(), 1);
    EXPECT_EQ(*sinks.begin(), inst5);
}


TEST_F(TestDesignDatabase, get_source_instance_port) {  // NOLINT
    load_str(R"(
module mod (
  input logic a,
  output logic d
);
endmodule
module top (
input logic a,
output logic d
);
mod dut (.*);
endmodule
)");
    auto const *dut = design_->get_instance("top.dut");
    auto const *top = design_->get_instance("top");
    auto source = design_->get_source_instances(dut);
    EXPECT_EQ(source.size(), 1);
    EXPECT_EQ(*source.begin(), top);
    auto sinks = design_->get_sink_instances(dut);
    EXPECT_EQ(sinks.size(), 1);
    EXPECT_EQ(*sinks.begin(), top);
}