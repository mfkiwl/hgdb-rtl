#include "../extern/slang/include/slang/syntax/SyntaxTree.h"
#include "../src/rtl.hh"
#include "gtest/gtest.h"
#include <memory>

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

TEST_F(TestDesignDatabase, parse_connection) {  // NOLINT
    load_str(R"(
module child (
    input logic a, b
    output logic c
);
endmodule
module top;
    logic a, b, c;
    childl inst (.*);
endmodule
)");

    auto const *p = design_->select("top");
    EXPECT_NE(p, nullptr);
}