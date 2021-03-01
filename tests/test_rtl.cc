#include <memory>

#include "../extern/slang/include/slang/syntax/SyntaxTree.h"
#include "../src/rtl.hh"
#include "gtest/gtest.h"

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
