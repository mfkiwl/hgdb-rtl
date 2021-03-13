#include "log.hh"

#include "fmt/format.h"

std::map<hgdb::log::LogIndex, hgdb::log::LogItem> LogItem::cached_items_;

std::map<std::string, pybind11::object> LogItem::values() const {
    hgdb::log::LogItem item = get_item();
    std::map<std::string, pybind11::object> result{{"time", py::cast(item.time)}};
    auto const fmt = *item.format;
    uint64_t int_values = 0, float_values = 0, str_values = 0;
    for (auto const &[name, value] : fmt) {
        auto const &[type, index] = value;
        switch (type) {
            case hgdb::log::LogFormatParser::ValueType::Hex:
            case hgdb::log::LogFormatParser::ValueType::Int:
            case hgdb::log::LogFormatParser::ValueType::Time: {
                auto v = py::cast(item.int_values[int_values++]);
                result.emplace(name, v);
                break;
            }
            case hgdb::log::LogFormatParser::ValueType::Float: {
                auto v = py::cast(item.float_values[float_values++]);
                result.emplace(name, v);
                break;
            }
            case hgdb::log::LogFormatParser::ValueType::Str: {
                auto v = py::cast(item.str_values[str_values++]);
                result.emplace(name, v);
                break;
            }
        }
    }
    return result;
}

hgdb::log::LogItem LogItem::get_item() const {
    if (cached_items_.find(index) != cached_items_.end()) {
        return cached_items_.at(index);
    } else {
        // need to get the item
        hgdb::log::LogItem item;
        db->get_item(&item, index);
        if (cached_items_.size() >= cache_size_) {
            // just erase the first one
            cached_items_.erase(cached_items_.begin());
        }
        cached_items_.emplace(index, item);
        return item;
    }
}

Log::Log() : DataSource(DataSourceType::Log) { db_ = std::make_unique<hgdb::log::LogDatabase>(); }

void Log::add_file(const std::string &filename,
                   const std::shared_ptr<hgdb::log::LogFormatParser> &parser) {
    files_.emplace_back(std::make_pair(filename, parser));
    parsers_.emplace_back(parser);
}

void Log::on_added(Ooze *ooze) {
    ooze_ = ooze;
    for (auto const &[filename, parser] : files_) {
        auto parser_index = db_->parse(filename, *parser);
        parser_batches_.emplace_back(parser_index);
    }
}

std::shared_ptr<QueryArray> Log::get_selector(py::handle handle) {
    // try out specific types first
    auto array = std::make_shared<QueryArray>(ooze_);
    for (auto i = 0u; i < parsers_.size(); i++) {
        auto obj = py::cast(parsers_[i]);
        if (handle.is(obj)) {
            // linear scan on item index and only pick the ones with batch index
            auto const &item_index = db_->item_index();
            auto const &batch_index = parser_batches_[i];
            for (auto const &index : item_index) {
                if (batch_index.find(index->batch_index) != batch_index.end()) {
                    auto ptr = std::make_shared<LogItem>(ooze_, db_.get(), *index);
                    array->add(ptr);
                }
            }
        }
    }
    if (!array->empty()) return array;

    if (handle.is(py::type::of<LogItem>())) {
        auto const &item_index = db_->item_index();
        for (auto const &index : item_index) {
            auto ptr = std::make_shared<LogItem>(ooze_, db_.get(), *index);
            array->add(ptr);
        }
        return array;
    }
    return nullptr;
}

std::vector<py::handle> Log::provides() const {
    std::vector<py::handle> result;
    for (auto const &parser : parsers_) {
        result.emplace_back(py::cast(parser));
    }
    result.emplace_back(py::type::of<LogItem>());
    return result;
}

auto log_item_init(const std::shared_ptr<hgdb::log::LogFormatParser> &parser,
                   const py::kwargs &kwargs) {
    hgdb::log::LogItem item;
    item.format = &parser->format;
    for (auto const &[name, value] : kwargs) {
        auto str_name = name.cast<std::string>();
        if (parser->format.find(str_name) == parser->format.end()) {
            throw py::value_error(fmt::format(
                "Unable to find {0}. Make sure you have the format setup via set_format()",
                str_name));
        }
        auto const &[f_type, f_index] = parser->format.at(str_name);
        auto expected_size = f_index + 1;
        if (std::string(value.ptr()->ob_type->tp_name) == "int") {
            // int type
            auto v = value.cast<int64_t>();
            if (f_type != hgdb::log::LogFormatParser::ValueType::Int) {
                throw py::value_error(
                    fmt::format("{0} is set to different type than int", str_name));
            }

            if (item.int_values.size() < expected_size) {
                item.int_values.resize(expected_size);
            }
            item.int_values[f_index] = v;
        } else if (std::string(value.ptr()->ob_type->tp_name) == "str") {
            // str type
            auto v = value.cast<std::string>();
            if (f_type != hgdb::log::LogFormatParser::ValueType::Str) {
                throw py::value_error(
                    fmt::format("{0} is set to different type than str", str_name));
            }

            if (item.str_values.size() < expected_size) {
                item.str_values.resize(expected_size);
            }
            item.str_values[f_index] = v;
        } else if (std::string(value.ptr()->ob_type->tp_name) == "float") {
            // float type
            auto v = value.cast<double>();
            if (f_type != hgdb::log::LogFormatParser::ValueType::Float) {
                throw py::value_error(
                    fmt::format("{0} is set to different type than float", str_name));
            }

            if (item.float_values.size() < expected_size) {
                item.float_values.resize(expected_size);
            }
            item.float_values[f_index] = v;
        }
    }
    return item;
}

void init_log_item(py::module &m) {
    auto item = py::class_<LogItem, QueryObject, std::shared_ptr<LogItem>>(m, "LogItem");
    item.def("__getattr__", [](const LogItem &item, const std::string &name) {
        auto log = item.get_item();
        auto const &format = *log.format;
        if (format.find(name) == format.end()) {
            throw GenericAttributeError(name);
        }
        auto [type, index] = format.at(name);
        switch (type) {
            case hgdb::log::LogFormatParser::ValueType::Hex:
            case hgdb::log::LogFormatParser::ValueType::Int:
            case hgdb::log::LogFormatParser::ValueType::Time: {
                return py::cast(log.int_values[index]);
            }
            case hgdb::log::LogFormatParser::ValueType::Float: {
                return py::cast(log.float_values[index]);
            }
            case hgdb::log::LogFormatParser::ValueType::Str: {
                return py::cast(log.str_values[index]);
            }
        }
        throw GenericAttributeError(name);
    });
    item.def_property_readonly("time", [](const LogItem &item) { return item.get_item().time; });

    // the actual log item that customer parser needs to provide
    auto log =
        py::class_<hgdb::log::LogItem, std::shared_ptr<hgdb::log::LogItem>>(m, "ParsedLogItem");
    log.def(py::init([](const std::shared_ptr<hgdb::log::LogFormatParser> &parser,
                        const py::kwargs &kwargs) { return log_item_init(parser, kwargs); }));
    log.def_readwrite("time", &hgdb::log::LogItem::time);
}

void init_log_data_source(py::module &m) {
    auto log = py::class_<Log, DataSource, std::shared_ptr<Log>>(m, "Log");
    log.def(py::init<>());
    log.def("add_file", &Log::add_file, py::arg("filename"), py::arg("parser"));
}

class PyLogParser : public hgdb::log::LogFormatParser {
public:
    using hgdb::log::LogFormatParser::LogFormatParser;

    hgdb::log::LogItem parse(const std::string &content) override {
        PYBIND11_OVERRIDE_PURE(hgdb::log::LogItem, hgdb::log::LogFormatParser, parse, content);
    }
};

void init_parser(py::module &m) {
    auto parser = py::class_<hgdb::log::LogFormatParser, PyLogParser,
                             std::shared_ptr<hgdb::log::LogFormatParser>>(m, "LogFormatParser");
    parser.def(py::init<>());
    parser.def("set_format", [](hgdb::log::LogFormatParser &parser, const py::kwargs &types) {
        // we generate the format underneath
        uint64_t int_values = 0, float_values = 0, str_values = 0;
        for (auto const &[py_name, obj] : types) {
            auto name = py_name.cast<std::string>();
            if (obj.is(py::int_().get_type())) {
                parser.format.emplace(
                    name, std::make_pair(hgdb::log::LogFormatParser::ValueType::Int, int_values++));
            } else if (obj.is(py::float_().get_type())) {
                parser.format.emplace(
                    name,
                    std::make_pair(hgdb::log::LogFormatParser::ValueType::Float, float_values++));
            } else if (obj.is(py::str().get_type())) {
                parser.format.emplace(
                    name, std::make_pair(hgdb::log::LogFormatParser::ValueType::Str, str_values++));
            } else {
                throw py::value_error(fmt::format("Invalid type for {0}", name));
            }
        }
        if (types.size() != parser.format.size()) {
            throw py::value_error("Missing parser value");
        }
    });
    parser.def("parse", &hgdb::log::LogFormatParser::parse, py::arg("string_content"));
    parser.def_property_readonly(
        "TYPE", [](const hgdb::log::LogFormatParser &parser) { return py::cast(parser); });

    auto scan = py::class_<hgdb::log::LogPrintfParser, hgdb::log::LogFormatParser,
                           std::shared_ptr<hgdb::log::LogPrintfParser>>(m, "LogPrintfParser");
    scan.def(py::init([](const std::string &format, const std::vector<std::string> &attr_names) {
                 auto parser = hgdb::log::LogPrintfParser(format, attr_names);
                 if (parser.has_error()) {
                     throw py::value_error("Unable to parse given format");
                 }
                 return parser;
             }),
             py::arg("format"), py::arg("attr_names"));
}

void init_log(py::module &m) {
    init_log_item(m);
    init_log_data_source(m);
    init_parser(m);
}
