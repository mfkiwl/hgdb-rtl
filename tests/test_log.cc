#include <iostream>

#include "../src/log.hh"
#include "fmt/format.h"
#include "gtest/gtest.h"

class DummyParser : public hgdb::log::LogFormatParser {
public:
    [[nodiscard]] hgdb::log::LogItem parse(const std::string &content) override {
        hgdb::log::LogItem item(count_);
        item.int_values = {count_, count_ * 10, count_ * 20};
        item.float_values = {count_ * 0.5, count_ * 1.5};
        item.str_values = {fmt::format("{0}", count_), fmt::format("{0}", count_ + 1)};
        count_++;
        return item;
    }

    [[nodiscard]] std::map<std::string, std::pair<ValueType, uint64_t>> format() const override {
        std::map<std::string, std::pair<ValueType, uint64_t>> result;
        result["a"] = {hgdb::log::LogFormatParser::ValueType::Int, 0};
        result["b"] = {hgdb::log::LogFormatParser::ValueType::Float, 1};
        result["c"] = {hgdb::log::LogFormatParser::ValueType::Str, 2};
        result["d"] = {hgdb::log::LogFormatParser::ValueType::Int, 3};
        result["e"] = {hgdb::log::LogFormatParser::ValueType::Float, 4};
        result["f"] = {hgdb::log::LogFormatParser::ValueType::Str, 5};
        result["g"] = {hgdb::log::LogFormatParser::ValueType::Int, 6};

        return result;
    }

private:
    int64_t count_ = 0;
};

TEST(log, test_parsing) {  // NOLINT
    std::stringstream ss;
    constexpr auto num_items = 3000;
    for (auto i = 0; i < num_items; i++) {
        ss << i << std::endl;
    }
    hgdb::log::LogDatabase db;
    db.add_file(ss);
    DummyParser parser;
    db.parse(parser);
    auto batches = db.get_batch(0);
    EXPECT_EQ(batches.size(), 1);
    auto const *batch = batches[0];
    std::vector<hgdb::log::LogItem> items;
    items.resize(batch->size());
    std::vector<hgdb::log::LogItem *> ptr_items;
    ptr_items.reserve(items.size());
    for (auto &item : items) ptr_items.emplace_back(&item);
    batch->get_items(ptr_items);
    EXPECT_EQ(ptr_items.size(), 1024);
    EXPECT_EQ(ptr_items[42]->time, 42);
    EXPECT_EQ(ptr_items[42]->str_values[0], "42");
    EXPECT_EQ(ptr_items[42]->float_values[1], 42 * 1.5);

    batches = db.get_batch(2500);
    EXPECT_EQ(batches.size(), 1);
    batch = batches[0];
    EXPECT_EQ(batch->size(), num_items % 1024);
}

TEST(log, test_printf) {  // NOLINT
    std::stringstream ss;
    constexpr auto num_items = 3000;
    for (auto i = 0; i < num_items; i++) {
        ss << fmt::format("@{0} PROC: {1} 0x{2:08X} aa.bb.cc", i + 1, i, i + 2) << std::endl;
    }
    hgdb::log::LogDatabase db;
    db.add_file(ss);
    auto parser = hgdb::log::LogPrintfParser("@%t PROC: %0d 0x%08X %m", {"proc", "value", "inst"});
    db.parse(parser);
    auto batches = db.get_batch(1);
    EXPECT_EQ(batches.size(), 1);
    auto const &batch = batches[0];

    std::vector<hgdb::log::LogItem> items;
    items.resize(batch->size());
    std::vector<hgdb::log::LogItem *> ptr_items;
    ptr_items.reserve(items.size());
    for (auto &item : items) ptr_items.emplace_back(&item);
    batch->get_items(ptr_items);

    EXPECT_EQ(ptr_items.size(), 1024);
    EXPECT_EQ(ptr_items[42]->time, 42 + 1);
    EXPECT_EQ(ptr_items[42]->int_values[0], 42);
    EXPECT_EQ(ptr_items[42]->int_values[1], 44);
    EXPECT_EQ(ptr_items[42]->str_values[0], "aa.bb.cc");
}