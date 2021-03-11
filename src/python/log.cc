#include "log.hh"

std::map<std::string, pybind11::object> LogItem::values() const {
    hgdb::log::LogItem item = get_item();
    std::map<std::string, pybind11::object> result{{"time", py::cast(item.time)}};
    auto const fmt = db->format();
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

std::vector<py::handle> Log::provides() const { return {py::type::of<LogItem>()}; }

void init_log_item(py::module &m) {
    auto item = py::class_<LogItem, QueryObject, std::shared_ptr<LogItem>>(m, "LogItem");
    item.def("__getattr__", [](const LogItem &item, const std::string &name) {
        auto log = item.get_item();
        auto const &format = item.db->format();
        if (format.find(name) == format.end()) {
            throw GenericAttributeError(name);
        }
    });
}

void init_log(py::module &m) { init_log_item(m); }
