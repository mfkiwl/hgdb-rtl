#include "vcd.hh"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

std::map<std::string, std::string> VCDSignal::values() const {
    return {{"name", name}, {"path", path}};
}

VCD::VCD(const std::string &path) : DataSource(DataSourceType::ValueChange) {
    db_ = std::make_unique<hgdb::vcd::VCDDatabase>(path);
}

std::shared_ptr<QueryArray> create_signal_array(const hgdb::vcd::VCDDatabase &db) {
    std::shared_ptr<QueryArray> result;
    for (auto const &[id, signal] : db.signals) {
        auto s = std::make_shared<VCDSignal>();
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
        return create_signal_array(*db_);
    }
    return nullptr;
}

void init_vcd(py::module &m) {
    auto vcd = py::class_<VCDSignal, QueryObject, std::shared_ptr<VCDSignal>>(m, "VCDSignal");
    vcd.def_property_readonly("path", [](const VCDSignal &s) { return s.path; });
    vcd.def_property_readonly("name", [](const VCDSignal &s) { return s.name; });

    auto source = py::class_<VCD, DataSource, std::shared_ptr<VCD>>(m, "VCD");
    source.def(py::init<const std::string>());
}