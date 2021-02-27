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