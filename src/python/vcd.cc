#include "vcd.hh"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <utility>

namespace py = pybind11;

std::map<std::string, py::object> VCDSignal::values() const {
    return {{"name", py::cast(name)}, {"path", py::cast(path)}};
}

VCD::VCD(std::string path) : DataSource(DataSourceType::ValueChange), filename_(std::move(path)) {}

std::shared_ptr<QueryArray> create_signal_array(Ooze *ooze, const hgdb::vcd::VCDDatabase &db) {
    auto result = std::make_shared<QueryArray>(ooze);
    for (auto const &[id, signal] : db.signals) {
        auto s = std::make_shared<VCDSignal>(ooze);
        s->name = signal->name;
        s->path = signal->path;
        s->signal = signal.get();
        result->add(s);
    }
    return result;
}

std::shared_ptr<QueryArray> VCD::get_selector(py::handle handle) {
    if (handle.is(py::type::of<VCDSignal>())) {
        // create instance selector
        return create_signal_array(ooze_, *db_);
    }
    return nullptr;
}

std::shared_ptr<QueryObject> VCD::bind(const std::shared_ptr<QueryObject> &obj,
                                       const py::object &type) {
    // maybe binding should have extra kwargs?
    auto py_obj = py::cast(obj);
    if (!py::hasattr(py_obj, "path")) return nullptr;
    auto const &py_path = py_obj.attr("path");
    auto const &path = py_path.cast<std::string>();
    if (db_->signals.find(path) == db_->signals.end()) {
        return nullptr;
    }
    if (!type.is(py::type::of<VCDSignal>())) {
        return nullptr;
    }
    auto const &signal = db_->signals.at(path);
    auto ptr = std::make_shared<VCDSignal>(ooze_);
    ptr->signal = signal.get();
    ptr->path = signal->path;
    ptr->name = signal->name;
    return ptr;
}

void VCD::on_added(Ooze *ooze) {
    ooze_ = ooze;
    parse();
}

void VCD::parse() { db_ = std::make_unique<hgdb::vcd::VCDDatabase>(filename_); }

class VCDValue : public QueryObject {
public:
    enum class ValueType { UInt, RawString };
    explicit VCDValue(Ooze *ooze, ValueType type, std::string path, uint64_t time,
                      const hgdb::vcd::VCDSignal *signal)
        : QueryObject(ooze), type(type), path(std::move(path)), time(time), signal(signal) {}
    ValueType type;
    std::string path;
    uint64_t time;

    const hgdb::vcd::VCDSignal *signal;
};

// we define UInt and str wrapper for VCD values
class UIntValue : public VCDValue {
public:
    UIntValue(Ooze *ooze, std::string path, uint64_t v, uint64_t time,
              const hgdb::vcd::VCDSignal *signal)
        : VCDValue(ooze, ValueType::UInt, std::move(path), time, signal), value(v) {}
    uint64_t value;

    [[nodiscard]] std::map<std::string, py::object> values() const override {
        return {{"path", py::cast(path)}, {"value", py::cast(value)}, {"time", py::cast(time)}};
    }
};

class StringValue : public VCDValue {
public:
    StringValue(Ooze *ooze, std::string path, std::string value, uint64_t time,
                const hgdb::vcd::VCDSignal *signal)
        : VCDValue(ooze, ValueType::RawString, std::move(path), time, signal),
          value(std::move(value)) {}

    std::string value;
    [[nodiscard]] std::map<std::string, py::object> values() const override {
        return {{"path", py::cast(path)}, {"value", py::cast(value)}, {"time", py::cast(time)}};
    }
};

std::function<std::shared_ptr<VCDValue>(const std::shared_ptr<VCDSignal> &)> get_value(
    uint64_t time, bool use_str = false) {
    auto func = [time, use_str](const std::shared_ptr<VCDSignal> &s) {
        std::shared_ptr<VCDValue> ptr;
        if (use_str) {
            auto v = s->signal->get_value(time);
            ptr = std::make_shared<StringValue>(s->ooze, s->path, v, time, s->signal);
        } else {
            auto v = s->signal->get_uint_value(time);
            ptr = std::make_shared<UIntValue>(s->ooze, s->path, v, time, s->signal);
        }
        return ptr;
    };

    return func;
}

class VCDTimeGenerator : public FilterMapperGenerator {
public:
    explicit VCDTimeGenerator(const std::set<uint64_t> &times) : times_(times), it(times.begin()) {}

    std::optional<std::function<std::shared_ptr<QueryObject>(QueryObject *)>> next() override {
        if (it == times_.end()) {
            return std::nullopt;
        } else {
            auto t = *it;
            it++;
            auto value = get_value(t);
            auto func = [value](QueryObject *obj) -> std::shared_ptr<QueryObject> {
                auto ptr = obj->shared_from_this();
                auto v = std::dynamic_pointer_cast<VCDSignal>(ptr);
                if (!v) return nullptr;
                auto r = value(v);
                return r;
            };
            return func;
        }
    }

private:
    const std::set<uint64_t> &times_;
    std::set<uint64_t>::const_iterator it;
};

std::unique_ptr<FilterMapperGenerator> VCD::filter_generator() const {
    return std::make_unique<VCDTimeGenerator>(db_->times);
}

std::shared_ptr<VCDValue> pre_value(const std::shared_ptr<VCDValue> &value) {
    auto *db = value->signal->db;
    auto time = value->time;
    // need to find previous time
    auto pre_time_iter = db->times.lower_bound(time);
    // find previous timestamp that has the value change
    pre_time_iter--;
    if (pre_time_iter != db->times.end()) {
        auto t = *pre_time_iter;
        switch (value->type) {
            case VCDValue::ValueType::RawString: {
                auto v = value->signal->get_value(t);
                return std::make_shared<StringValue>(value->ooze, value->path, v, t, value->signal);
            }
            case VCDValue::ValueType::UInt: {
                auto v = value->signal->get_uint_value(t);
                return std::make_shared<UIntValue>(value->ooze, value->path, v, t, value->signal);
            }
            default: {
                return nullptr;
            }
        }
    } else {
        return nullptr;
    }
}

void init_vcd(py::module &m) {
    auto vcd = py::class_<VCDSignal, QueryObject, std::shared_ptr<VCDSignal>>(m, "VCDSignal");
    vcd.def_property_readonly("path", [](const VCDSignal &s) { return s.path; });
    vcd.def_property_readonly("name", [](const VCDSignal &s) { return s.name; });

    auto source = py::class_<VCD, DataSource, std::shared_ptr<VCD>>(m, "VCD");
    source.def(py::init<const std::string>(), py::arg("filename"));
    source.def_property_readonly("stats", [](const VCD &vcd) { return vcd.get_stats(); });

    m.def("get_value", &get_value, py::arg("time"), py::arg("use_str"));
    m.def(
        "get_value", [](uint64_t time) { return get_value(time, false); }, py::arg("time"));
    m.def("pre_value", &pre_value, py::arg("value"));

    auto value = py::class_<VCDValue, QueryObject, std::shared_ptr<VCDValue>>(m, "VCDValue");
    value.def_property_readonly("path", [](const VCDValue &v) { return v.path; });
    value.def_property_readonly("time", [](VCDValue &v) { return v.time; });

    auto uint = py::class_<UIntValue, VCDValue, std::shared_ptr<UIntValue>>(m, "UIntValue");
    uint.def("__int__", [](const UIntValue &v) { return v.value; });
    uint.def_property_readonly("value", [](const UIntValue &v) { return v.value; });

    auto str = py::class_<StringValue, VCDValue, std::shared_ptr<StringValue>>(m, "StringValue");
    str.def("__str__", [](const StringValue &s) { return s.value; });
    str.def_property_readonly("value", [](const StringValue &v) { return v.value; });
}